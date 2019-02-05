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

#include "App.h"
#include "OVR_FileSys.h"

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
		// gl_FragColor = outColor * texture2D( Texture0, oTexCoord );
		gl_FragColor = outColor;
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

ovrRibbon::ovrRibbon( const ovrPointList & pointList, const float width, const Vector4f & color, App*app )
	: Color( color )
{
	fucking_app = app;
    MemBufferT< uint8_t > parmBuffer;
    if ( !fucking_app->GetFileSys().ReadFile( "apk:///assets/faces.csv", parmBuffer ) )
    {
        OVR_LOG( "fuck yyy Failed to load file!!" );
    } else
    {   // this is ridiculous: adding final 0 to make null terminated string
        //size_t newSize = parmBuffer.GetSize() + 1;
        //uint8_t * temp = new uint8_t[newSize];
        //memcpy( temp, static_cast< uint8_t* >( parmBuffer ), parmBuffer.GetSize() );
        //temp[parmBuffer.GetSize()] = 0;
        //parmBuffer.TakeOwnershipOfBuffer( *(void**)&temp, newSize );
        // end of ridiculous section

        uint8_t * temp2 = static_cast< uint8_t* >( parmBuffer );
        OVR_LOG( "yyy fuck ok opened ok!! size %d buffer=%s", (int)parmBuffer.GetSize(),temp2);
        float culo = -1.0;
        char*data = (char*) temp2;
        int offset;
        int count = 0;

        while (sscanf(data, " %f,%n", &culo, &offset) == 1)
        {
            data += offset;
            OVR_LOG("yyy ribbon Read   |%f|      count %d\n", culo, count);
            count++;
        }
    }


    // initialize the surface geometry
	const int maxPoints = pointList.GetMaxPoints();
	OVR_LOG("max points %d", maxPoints);
	const int maxTris = ( maxPoints - 1 );
	const int numVerts = maxTris * 3;
	
	VertexAttribs attr;
	attr.position.Resize( numVerts );
	attr.color.Resize( numVerts );
	attr.uv0.Resize( numVerts );

	// the indices will never change
	const int numIndices = maxTris * 3;
	Array< TriangleIndex> indices;
	indices.Resize( numIndices );
	// so we can just set them up at initialization time
	TriangleIndex v = 0;
	for ( int i = 0; i < maxTris; ++i )
	{
		indices[i * 3 + 0] = v + 0;
		indices[i * 3 + 1] = v + 1;
		indices[i * 3 + 2] = v + 2;
		v += 3;
	}

	Surface.geo.Create( attr, indices );
	Surface.geo.primitiveType = GL_TRIANGLES;
	Surface.geo.indexCount = 0;

	// initialize the rest of the surface 
	Surface.surfaceName = "ribbon";
	Surface.numInstances = 1;

	ovrGraphicsCommand & gc = Surface.graphicsCommand;

	Texture = CreateRibbonTexture();
#if 0  // To-Do was 1, set to 0 to disable texturing (see also how I modified the fragment shader)
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
	gpu.depthMaskEnable = true;
    gpu.blendEnable = ovrGpuState::BLEND_ENABLE;
	gpu.blendSrc = GL_SRC_ALPHA;
	gpu.blendDst = GL_ONE_MINUS_SRC_ALPHA;
	gpu.blendSrcAlpha = GL_SRC_ALPHA;
	gpu.blendDstAlpha = GL_ONE_MINUS_SRC_ALPHA;
	gpu.cullEnable = false; // TODO true;
}

ovrRibbon::~ovrRibbon()
{
	DeleteTexture( Texture );
	GlProgram::Free( Surface.graphicsCommand.Program );
	Surface.geo.Free();
}

void ovrRibbon::AddPoint( ovrPointList & pointList, const ovrVector3f & point )
{
}

void ovrRibbon::Update( const ovrPointList & pointList, const bool invertAlpha )  // TODO
{
	if ( pointList.GetCurPoints() <= 1 )
	{
		//OVR_LOG( "Ribbon: empty" );
		return;
	}

	//Vector3f eyePos( GetViewMatrixPosition( centerViewMatrix ) );
	// Vector3f eyeFwd( GetViewMatrixForward( centerViewMatrix ) );
/*
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
*/
    // Vector3f edgeDir = getEdgeDir2( eyeFwd, *curPoint, *nextPoint );
    // float alpha = calcAlpha( curEdge, pointList.GetCurPoints(), invertAlpha );
    //Vector3f edgeDir(0.0, 1.0, 0.0);
    //edgeDir = edgeDir.Normalized();

    VertexAttribs attr;
    const int curPoints = pointList.GetCurPoints();
    const int numVerts = ( curPoints ) * 1;
    attr.position.Resize( numVerts );
    attr.color.Resize( numVerts );
    attr.uv0.Resize( numVerts );
    int numV = 0;
    int curIdx = pointList.GetFirst();

	float alpha = 1.0;

	for ( ; ; )
	{
        const Vector3f * curPoint = &pointList.Get( curIdx );
        curPoint = &pointList.Get( curIdx );

        // OVR_LOG("%d cva %.1f, %.1f, %.1f", numV, curPoint->x, curPoint->y, curPoint->z);

		attr.position[(numV)]	= *curPoint;
		attr.color[(numV)]		= Vector4f( Color.x, Color.y, Color.z, alpha );
        attr.uv0[(numV)] 		= OVR::Vector2f( 1.0f, 0.0f );

		curIdx = pointList.GetNext( curIdx );
		if ( curIdx < 0 )
		{
			break;
		}

		numV++;

		// alpha = calcAlpha( curEdge, pointList.GetCurPoints(), invertAlpha );
	}

	// update the vertices
	Surface.geo.Update( attr, false );
	Surface.geo.indexCount = numVerts;
}

void ovrRibbon::GenerateSurfaceList( Array< ovrDrawSurface > & surfaceList ) const
{
    // OVR_LOG("ovrRibbon::GenerateSurfaceList");
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
