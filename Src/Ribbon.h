/************************************************************************************

Filename    :   Ribbon.h
Content     :   Class that renders connected polygon strips from a list of points
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include "PointList.h"
#include "SurfaceRender.h"

namespace OVR {

class App;

//==============================================================
// ovrRibbon
class ovrRibbon
{
public:
	ovrRibbon( const ovrPointList & pointList, const float width, const Vector4f & color, App*app );
	~ovrRibbon();
    App *fucking_app;
	void AddPoint( ovrPointList & pointList, const ovrVector3f & point );
	void Update( std::vector<float> &coords  );
	void GenerateSurfaceList( Array< ovrDrawSurface > & surfaceList ) const;

private:
	Vector4f 						Color;
	ovrSurfaceDef					Surface;
	GlTexture						Texture;
};

} // namespace OVR
