/************************************************************************************

Filename    :   Ribbon.h
Content     :   Class that renders connected polygon strips from a list of points
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "Ribbon.h"
#include "Kernel/OVR_LogUtils.h"
#include "GlTexture.h"
#include "VrCommon.h"

namespace OVR {

static const char* ribbonVertexShader = R"=====(
	attribute vec4 Position;
	attribute vec4 VertexColor;
	attribute vec2 TexCoord;
	varying lowp vec4 outColor;
	varying highp vec2 oTexCoord;
	void main()
	{
		gl_Position = TransformVertex( Position );
		oTexCoord = TexCoord;
		outColor = VertexColor;
	}
)=====";

static const char* ribbonFragmentShader = R"=====(
	uniform sampler2D Texture0;
	varying lowp vec4 outColor;
	varying highp vec2 oTexCoord;
	void main()
	{
		gl_FragColor = outColor * texture2D( Texture0, oTexCoord );
		//gl_FragColor = outColor;
	}
)=====";
//==============================================================================================
// ovrRibbon

static GlTexture CreateRibbonTexture()
{
	const int TEX_WIDTH = 64;
	const int TEX_HEIGHT = 64;
	const int TEX_SIZE = TEX_WIDTH * TEX_HEIGHT;
	uint32_t * tex = new uint32_t[TEX_SIZE];
	for ( int y = 0; y < TEX_HEIGHT; ++y )
	{
		const uint32_t alpha = ( y < 16 ) ? y * 16 : ( y > 48 ) ? ( TEX_HEIGHT - y ) * 16 : 0xff;
		const uint32_t color = ( alpha << 24 ) | 0xffffff;
		for ( int x = 0; x < TEX_WIDTH; ++x )
		{
			tex[( y * TEX_WIDTH ) + x] = color;
		}
	}	
	GlTexture glTexture = LoadRGBATextureFromMemory( reinterpret_cast< uint8_t*>( tex ), TEX_WIDTH, TEX_HEIGHT, false );	
	delete [] tex;
	return glTexture;
}

ovrRibbon::ovrRibbon( const ovrPointList & pointList, const float width, const Vector4f & color )
	: HalfWidth( width )
	, Color( color )
{
	// initialize the surface geometry
	const int maxPoints = pointList.GetMaxPoints();
	const int maxQuads = ( maxPoints - 1 );
	const int numVerts = maxQuads * 4;
	
	VertexAttribs attr;
	attr.position.Resize( numVerts );
	attr.color.Resize( numVerts );
	attr.uv0.Resize( numVerts );

	// the indices will never change
	const int numIndices = maxQuads * 6;
	Array< TriangleIndex> indices;
	indices.Resize( numIndices );
	// so we can just set them up at initialization time
	TriangleIndex v = 0;
	for ( int i = 0; i < maxQuads; ++i )
	{
		indices[i * 6 + 0] = v + 0;
		indices[i * 6 + 1] = v + 1;
		indices[i * 6 + 2] = v + 2;
		indices[i * 6 + 3] = v + 2;
		indices[i * 6 + 4] = v + 1;
		indices[i * 6 + 5] = v + 3;
		v += 4;
	}

	Surface.geo.Create( attr, indices );
	Surface.geo.primitiveType = GL_TRIANGLES;
	Surface.geo.indexCount = 0;

	// initialize the rest of the surface 
	Surface.surfaceName = "ribbon";
	Surface.numInstances = 1;

	ovrGraphicsCommand & gc = Surface.graphicsCommand;

	Texture = CreateRibbonTexture();
#if 1
	gc.UniformData[0].Data = &Texture;

	ovrProgramParm parms[] =
	{
		{ "Texture0",		ovrProgramParmType::TEXTURE_SAMPLED },
	};

	gc.Program = GlProgram::Build( ribbonVertexShader, ribbonFragmentShader, &parms[0], sizeof( parms ) / sizeof( ovrProgramParm ) );
#else
	gc.Program = GlProgram::Build( ribbonVertexShader, ribbonFragmentShader, nullptr, 0 );
#endif

	if ( !Surface.graphicsCommand.Program.IsValid() )
	{
		OVR_LOG( "Error building ribbon gpu program" );
	}

	ovrGpuState & gpu = gc.GpuState;	
	gpu.depthEnable = true;
	gpu.depthMaskEnable = false;
	gpu.blendEnable = ovrGpuState::BLEND_ENABLE;
	gpu.blendSrc = GL_SRC_ALPHA;
	gpu.blendDst = GL_ONE_MINUS_SRC_ALPHA;
	gpu.blendSrcAlpha = GL_SRC_ALPHA;
	gpu.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
	gpu.cullEnable = true;
}

ovrRibbon::~ovrRibbon()
{
	DeleteTexture( Texture );
	GlProgram::Free( Surface.graphicsCommand.Program );
	Surface.geo.Free();
}

void ovrRibbon::AddPoint( ovrPointList & pointList, const ovrVector3f & point )
{
	//OVR_LOG( "Ribbon: AddPoint" );
	if ( pointList.IsFull() )
	{
		return;
	}

	// don't add points really close together
	const int lastIndex = pointList.GetLast();
	const Vector3f & curTail = pointList.Get( lastIndex );

	float d = ( curTail - point ).LengthSq();
	if ( d < 0.0001f )
	{
		return;
	}
	pointList.AddToTail( point );
}

void ovrRibbon::Update( const ovrPointList & pointList, const ovrMatrix4f & centerViewMatrix, const bool invertAlpha )
{
	//OVR_LOG( "Update: this == %p", this );

	if ( pointList.GetCurPoints() <= 1 )
	{
		//OVR_LOG( "Ribbon: empty" );
		return;
	}

	VertexAttribs attr;
	const int curPoints = pointList.GetCurPoints();
	const int numVerts = ( curPoints - 1 ) * 4;
	attr.position.Resize( numVerts );
	attr.color.Resize( numVerts );
	attr.uv0.Resize( numVerts );

	//Vector3f eyePos( GetViewMatrixPosition( centerViewMatrix ) );
	Vector3f eyeFwd( GetViewMatrixForward( centerViewMatrix ) );
	int numQuads = 0;
	int curEdge = 1;
	int curIdx = pointList.GetFirst();
	int nextIdx = pointList.GetNext( curIdx );

	auto getEdgeDir2 = []( const Vector3f & eyeFwd, const Vector3f & cur, const Vector3f & next )
	{
		Vector3f dir = next - cur;
		Vector3f proj = ( dir - ( eyeFwd * dir.Dot( eyeFwd ) ) ).Normalized();
		Vector3f cross = proj.Cross( eyeFwd );
		return cross;
	};

	auto calcAlpha = []( const int curEdge, const int curPoints, const bool invertAlpha )
	{
		if ( invertAlpha )
		{
			return 1.0f - Alg::Clamp( (float)( curEdge >> 1 ) / (float)( curPoints ), 0.0f, 1.0f );
		}
		else
		{
			return Alg::Clamp( (float)curEdge / (float)( curPoints >> 1 ), 0.0f, 1.0f );
		}
	};

	const Vector3f * curPoint = &pointList.Get( curIdx );
	const Vector3f * nextPoint = &pointList.Get( nextIdx );

	Vector3f edgeDir = getEdgeDir2( eyeFwd, *curPoint, *nextPoint );
	float alpha = calcAlpha( curEdge, pointList.GetCurPoints(), invertAlpha );
	
	// cur edge
	attr.position[(numQuads * 4) + 0] 	= *curPoint + (edgeDir * HalfWidth);
	attr.color[(numQuads * 4) + 0]		= Vector4f( Color.x, Color.y, Color.z, alpha );
	attr.position[(numQuads * 4) + 1]	= *curPoint - (edgeDir * HalfWidth);
	attr.color[(numQuads * 4) + 1]		= Vector4f( Color.x, Color.y, Color.z, alpha );
	attr.uv0[(numQuads * 4) + 0] 		= OVR::Vector2f( 0.0f, 0.0f );
	attr.uv0[(numQuads * 4) + 1] 		= OVR::Vector2f( 0.0f, 1.0f );

	for ( ; ; )
	{
		curPoint = &pointList.Get( curIdx );
		nextPoint = &pointList.Get( nextIdx );

		edgeDir = getEdgeDir2( eyeFwd, *curPoint, *nextPoint );
		curEdge++;
		alpha = calcAlpha( curEdge, pointList.GetCurPoints(), invertAlpha );

		// current quad next edge
		attr.position[(numQuads * 4) + 2]	= *nextPoint + (edgeDir * HalfWidth * alpha );
		attr.color[(numQuads * 4) + 2]		= Vector4f( Color.x, Color.y, Color.z, alpha );
		attr.position[(numQuads * 4) + 3]	= *nextPoint - (edgeDir * HalfWidth * alpha );
		attr.color[(numQuads * 4) + 3]		= Vector4f( Color.x, Color.y, Color.z, alpha );

		attr.uv0[(numQuads * 4) + 2] 		= OVR::Vector2f( 1.0f, 0.0f );
		attr.uv0[(numQuads * 4) + 3] 		= OVR::Vector2f( 1.0f, 1.0f );

		curIdx = nextIdx;
		nextIdx = pointList.GetNext( nextIdx );
		if ( nextIdx < 0 )
		{
			break;
		}

		numQuads++;

		alpha = calcAlpha( curEdge, pointList.GetCurPoints(), invertAlpha );

		// next quad first edge
		attr.position[(numQuads * 4) + 0]	= *nextPoint + (edgeDir * HalfWidth * alpha );
		attr.color[(numQuads * 4) + 0]		= Vector4f( Color.x, Color.y, Color.z, alpha );
		attr.position[(numQuads * 4) + 1]	= *nextPoint - (edgeDir * HalfWidth * alpha );
		attr.color[(numQuads * 4) + 1]		= Vector4f( Color.x, Color.y, Color.z, alpha );
		attr.uv0[(numQuads * 4) + 0] 		= OVR::Vector2f( 0.0f, 0.0f );
		attr.uv0[(numQuads * 4) + 1] 		= OVR::Vector2f( 0.0f, 1.0f );
	}

	//OVR_LOG( "Ribbon: %i points, %i edges, %i quads", pointList.GetCurPoints(), curEdge, numQuads );
	// update the vertices
	Surface.geo.Update( attr, false );
	Surface.geo.indexCount = numQuads * 6;
}

void ovrRibbon::GenerateSurfaceList( Array< ovrDrawSurface > & surfaceList ) const
{
	if ( Surface.geo.indexCount == 0 )
	{
		return;
	}

	ovrDrawSurface drawSurf;
	drawSurf.modelMatrix = Matrix4f::Identity();
	drawSurf.surface = &Surface;

	surfaceList.PushBack( drawSurf );
}

} // namespace OVR
