/*
   CRT - Guest - NTSC - Pass1
   
   Copyright (C) 2018-2022 guest(r) - guest.r@gmail.com

   Incorporates many good ideas and suggestions from Dr. Venom.
   I would also like give thanks to many Libretro forums members for continuous feedback, suggestions and caring about the shader.
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   
*/

layout(push_constant) uniform Push
{
	vec4 SourceSize;
	vec4 OriginalSize;
	vec4 OutputSize;
	uint FrameCount;
	float SIGMA_HOR;
	float HSHARPNESS;
	float S_SHARP;
	float HARNG;
	float HSHARP;
	float NTSCspike;
} params;

//---------------------------------------------------------------------------------------------------
// NTSC Filtering
//---------------------------------------------------------------------------------------------------
#pragma parameter HSM_NTSC_FILTERING_EMPTY_LINE		" " 0 0 0.001 0.001
#pragma parameter HSM_NTSC_FILTERING_TITLE 	"[ NTSC FILTERING OPTIONS ]: " 0.0 0.0 1.0 1.0

#pragma parameter HSHARPNESS 		"          NTSC Horizontal Filter Range -- HSHARPNESS" 1.5 1.0 8.0 0.05
#define HSHARPNESS params.HSHARPNESS

#pragma parameter SIGMA_HOR 		"          NTSC Horizontal Blur Sigma -- SIGMA_HOR" 0.90 0.1 7.0 0.05
#define SIGMA_HOR params.SIGMA_HOR

#pragma parameter S_SHARP 			"          NTSC Substractive Sharpness -- S_SHARP" 0.9 0.0 2.0 0.10
#define S_SHARP params.S_SHARP

#pragma parameter HSHARP 			"          NTSC Sharpness Definition -- HSHARP" 1.2 0.0 2.0 0.10
#define HSHARP params.HSHARP

#pragma parameter HARNG 			"          NTSC Substractive Sharpness Ringing -- HARNG" 0.4 0.0 4.0 0.10
#define HARNG params.HARNG

#pragma parameter NTSCspike 		"          NTSC Scanline Spike Removal -- NTSCspike" 1.0 0.0 2.0 0.10
#define spike params.NTSCspike

#define COMPAT_TEXTURE(c,d) texture(c,d)

#define TEX0 vTexCoord

#define OutputSize params.OutputSize
#define gl_FragCoord (vTexCoord * OutputSize.xy)

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
   gl_Position = global.MVP * Position;
   vTexCoord = TexCoord * 1.00001;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D LinearizePass;
layout(set = 0, binding = 3) uniform sampler2D InfoCachePass;

float invsqrsigma = 1.0/(2.0*SIGMA_HOR*SIGMA_HOR);

float gaussian(float x)
{
	return exp(-x*x*invsqrsigma);
}

void main()
{	
		// HSM Added
	vec2 viewportCoordTransformed = HSM_GetViewportCoordWithZoomAndPan(vTexCoord);
	HSM_UpdateGlobalScreenValuesFromCache(InfoCachePass, vTexCoord);

// If it's the potato preset render the whole frame
#ifndef IS_POTATO_PRESET
#ifndef IS_NO_REFLECT_PRESET
	// Have to get the scale of the coordinates so we can figure out the size of the onscreen rectangle of the area 
	HSM_GetBezelCoords(TUBE_DIFFUSE_COORD, 
						TUBE_DIFFUSE_SCALE, 
						TUBE_SCALE, 
						SCREEN_ASPECT, 
						false,
						BEZEL_OUTSIDE_SCALE,
						BEZEL_OUTSIDE_COORD, 
						BEZEL_OUTSIDE_CURVED_COORD, 
						FRAME_OUTSIDE_CURVED_COORD);
#endif
#endif

	/* HSM Removed
	vec2 prescalex = vec2(textureSize(LinearizePass, 0)) / params.OriginalSize.xy;

	vec4 SourceSize = params.OriginalSize * vec4(2.0*prescalex.x, prescalex.y, 0.5/prescalex.x, 1.0/prescalex.y);
	*/

	vec2 prescalex = global.NegativeCropAddedPassSize.xy / params.OriginalSize.xy;

	vec4 SourceSize = global.NegativeCropAddedPassSize * vec4(2.0*prescalex.x, prescalex.y, 0.5/prescalex.x, 1.0/prescalex.y);

	float f = fract(SourceSize.x * vTexCoord.x);
	f = 0.5 - f;
	vec2 tex = floor(SourceSize.xy * vTexCoord)*SourceSize.zw + 0.5*SourceSize.zw;
	vec3 color = 0.0.xxx;
	float scolor = 0.0;
	vec2 dx  = vec2(SourceSize.z, 0.0);

	float w = 0.0;
	float swsum = 0.0;
	float wsum = 0.0;
	vec3 pixel;

	vec3 cmax = 0.0.xxx;
	vec3 cmin = 1.0.xxx;
	float sharp = gaussian(HSHARPNESS) * S_SHARP;
	float maxsharp = 0.20;
	float FPR = HSHARPNESS;
	float fpx = 0.0;
	float sp = 0.0;
	float sw = 0.0;

	float ts = 0.025;
	vec3 luma = vec3(0.2126, 0.7152, 0.0722); 

	float LOOPSIZE = ceil(2.0*FPR);
	float CLAMPSIZE = round(2.0*LOOPSIZE/3.0);
	
	float n = -LOOPSIZE;
	
	do
	{
		pixel  = COMPAT_TEXTURE(LinearizePass, tex + n*dx).rgb;
		sp = max(max(pixel.r,pixel.g),pixel.b);
		
		w = gaussian(n+f) - sharp;
		fpx = abs(n+f-sign(n)*FPR)/FPR;
		if (abs(n) <= CLAMPSIZE) { cmax = max(cmax, pixel); cmin = min(cmin, pixel); }
		if (w < 0.0) w = clamp(w, mix(-maxsharp, 0.0, pow(clamp(fpx,0.0,1.0), HSHARP)), 0.0);
	
		color = color + w * pixel;
		wsum  = wsum + w;

		sw = max(w, 0.0) * (dot(pixel,luma) + ts); 
		scolor = scolor + sw * sp;
		swsum = swsum + sw;
		
		n = n + 1.0;
			
	} while (n <= LOOPSIZE);

	color = color / wsum;
	scolor = scolor / swsum;
	
	color = clamp(mix(clamp(color, cmin, cmax), color, HARNG), 0.0, 1.0); 
	
	scolor = clamp(mix(max(max(color.r, color.g),color.b), scolor, spike), 0.0, 1.0);
	
	FragColor = vec4(color, scolor);
} 