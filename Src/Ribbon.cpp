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
	attribute lowp vec4 Position;
	attribute lowp vec3 Normal;
	attribute vec4 VertexColor;
	varying lowp vec4 outColor;
    varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
    vec3 multiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,
			m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,
			m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );
	}
	vec3 transposeMultiply( mat4 m, vec3 v )
	{
		return vec3(
			m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,
			m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,
			m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );
	}
	void main()
	{
		gl_Position = TransformVertex( Position );
        vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );
		oEye = eye - vec3( ModelMatrix * Position );
		oNormal = multiply( ModelMatrix, Normal );
		outColor = VertexColor;
	}
)=====";

static const char* ribbonFragmentShader = R"=====(
	varying lowp vec4 outColor;
    varying lowp vec3 oEye;
	varying lowp vec3 oNormal;
    uniform lowp vec3 SpecularLightDirection;
	uniform lowp vec3 SpecularLightColor;
	uniform lowp vec3 AmbientLightColor;
	void main()
	{
		lowp vec3 eyeDir = normalize( oEye.xyz );
		lowp vec3 Normal = normalize( oNormal );
		lowp vec3 reflectionDir = dot( eyeDir, Normal ) * 2.0 * Normal - eyeDir;
		lowp vec4 diffuse = outColor;
		lowp vec3 ambientValue = diffuse.xyz * AmbientLightColor;

		lowp float nDotL = max( dot( Normal , SpecularLightDirection ), 0.0 );
		lowp vec3 diffuseValue = diffuse.xyz * SpecularLightColor * nDotL;

		lowp float specularPower = 1.0f - diffuse.a;
		specularPower = specularPower * specularPower;

		lowp vec3 H = normalize( SpecularLightDirection + eyeDir );
		lowp float nDotH = max( dot( Normal, H ), 0.0 );
		lowp float specularIntensity = pow( nDotH, 64.0f * ( specularPower ) ) * specularPower;
		lowp vec3 specularValue = specularIntensity * SpecularLightColor;

		lowp vec3 controllerColor = diffuseValue + ambientValue + specularValue;
        gl_FragColor.xyz = controllerColor;
        //gl_FragColor.xyz = diffuse.xyz;
        gl_FragColor.w = 1.0f;
	}
)=====";
//==============================================================================================
// ovrRibbon

ovrRibbon::ovrRibbon( const ovrPointList & pointList, const float width, const Vector4f & color, App*app )
	: Color( color )
{
    std::vector<float> coords;
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
        OVR_LOG( "yyy fuck ok opened ok!! size %d", (int)parmBuffer.GetSize()/*,temp2*/);
        float coord;
        char*data = (char*) temp2;
        int offset;
        int count = 0;

        while (sscanf(data, " %f,%n", &coord, &offset) == 1)
        {
            coords.push_back(coord);
            data += offset;
            // OVR_LOG("yyy ribbon Read   |%f|      count %d\n", coord, count);
            count++;
        }
        OVR_LOG( "yyy vector size %lu count=%d first=%f last =%f", coords.size(), count, coords.at(0), coords.back());
        // for(int n : coords) {
        //     std::cout << n << '\n';
    }

    // initialize the surface geometry
	//const int maxPoints = pointList.GetMaxPoints();
	//OVR_LOG("max points %d", maxPoints);
	//const int maxTris = ( maxPoints - 1 );
	const int numVerts = coords.size() / 3;
    OVR_LOG("yyy numVerts = %d", numVerts);
	
	VertexAttribs attr;
	attr.position.Resize( numVerts );
    attr.normal.Resize( numVerts );
	attr.color.Resize( numVerts );

	// the indices will never change
	Array< TriangleIndex> indices;
	indices.Resize( numVerts );
	// so we can just set them up at initialization time
	for ( int i = 0; i < numVerts; ++i )
	    indices[i] = (TriangleIndex)i;

	Surface.geo.Create( attr, indices );
	Surface.geo.primitiveType = GL_TRIANGLES;
	Surface.geo.indexCount = 0;

	// initialize the rest of the surface 
	Surface.surfaceName = "ribbon";
	Surface.numInstances = 1;

	ovrGraphicsCommand & gc = Surface.graphicsCommand;

    static Vector3f SpecularLightDirection = Vector3f( 0.0f, -1.0f, 0.0f );
    static Vector3f SpecularLightColor = Vector3f( 1.0f, 0.0f, 1.0f ) * 0.75f;
    static Vector3f AmbientLightColor = Vector3f( 1.0f, 1.0f, 1.0f ) * 0.85f;

    gc.UniformData[0].Data = &SpecularLightDirection;
	gc.UniformData[1].Data = &SpecularLightColor;
	gc.UniformData[2].Data = &AmbientLightColor;

	static ovrProgramParm parms[] =
	{
		    { "SpecularLightDirection",	ovrProgramParmType::FLOAT_VECTOR3 },
			{ "SpecularLightColor",		ovrProgramParmType::FLOAT_VECTOR3 },
			{ "AmbientLightColor",		ovrProgramParmType::FLOAT_VECTOR3 },
	};

	gc.Program = GlProgram::Build( ribbonVertexShader, ribbonFragmentShader, parms, sizeof( parms ) / sizeof( ovrProgramParm ) );
	
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

	Update(coords);
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

void ovrRibbon::Update(std::vector<float> &coords )  // TODO
{
    VertexAttribs attr;

    const int numVerts = coords.size() / 3;

    attr.position.Resize( numVerts );
    attr.normal.Resize( numVerts );
    attr.color.Resize( numVerts );

    Vector3f pA;
    Vector3f pB;
    Vector3f pC;
    Vector3f cb;
    Vector3f ab;

	for (unsigned int numTri = 0; numTri < coords.size() / 9; numTri++)
    {
        pA.x = coords.at(numTri * 9 + 0) / 30.0 + 1.0;
        pA.y = coords.at(numTri * 9 + 2) / 30.0 + 1.275;
        pA.z = -coords.at(numTri * 9 + 1) / 30.0 - 2.0;

        pB.x = coords.at(numTri * 9 + 3) / 30.0 + 1.0;
        pB.y = coords.at(numTri * 9 + 5) / 30.0 + 1.275;
        pB.z = -coords.at(numTri * 9 + 4) / 30.0 - 2.0;

        pC.x = coords.at(numTri * 9 + 6) / 30.0 + 1.0;
        pC.y = coords.at(numTri * 9 + 8) / 30.0 + 1.275;
        pC.z = -coords.at(numTri * 9 + 7) / 30.0 - 2.0;

        cb = pB - pC;
        ab = pB - pA;
        cb = cb.Cross(ab);
        cb.Normalize();

        attr.position[numTri * 3 + 0] = pA;
        attr.normal[numTri * 3 + 0]   = cb;
        attr.color[numTri * 3 + 0]    = Vector4f( Color.x, Color.y, Color.z, 1.0 );

        attr.position[numTri * 3 + 1] = pB;
        attr.normal[numTri * 3 + 1]   = cb;
        attr.color[numTri * 3 + 1]    = Vector4f( Color.x, Color.y, Color.z, 1.0 );

        attr.position[numTri * 3 + 2] = pC;
        attr.normal[numTri * 3 + 2]   = cb;
        attr.color[numTri * 3 + 2]    = Vector4f( Color.x, Color.y, Color.z, 1.0 );
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
