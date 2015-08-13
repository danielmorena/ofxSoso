/*
RoundedArcObject

Simple Object that draws an arc.

Soso OpenFrameworks Addon
Copyright (C) 2012 Sosolimited

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

#pragma once

#include "soso/ArcObject.h"

namespace soso
{

class RoundedArcObject : public ArcObject{

public:
	RoundedArcObject(float iOuterRadius, float iInnerRadius, float iStartAngle, float iEndAngle);
	~RoundedArcObject();

	void						render();
	//void						setResolution(int iRes);

	//void						setStartAngle(float iAngle);
	//void						setEndAngle(float iAngle);

public:

	float						curveRadius, curveRadius2;
	float						newStartAngle;
	float						newEndAngle;
	float						newInnerRadius;
	float						newOuterRadius;
	float						deltaAngle;

	ci::vec3					circPos1, circPos2,
								circPos3, circPos4;

	float						height;

};

} // namespace soso
