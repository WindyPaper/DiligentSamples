RWTexture2D<float3> Displacement;
RWTexture2D<float4> Derivatives;
RWTexture2D<float4> Turbulence;

Texture2D<float2> DxDz;
Texture2D<float2> DyDxz;
Texture2D<float2> DyxDyz;
Texture2D<float2> DxxDzz;

cbuffer cbResultMergeBuffer
{
	float Lambda;
	float DeltaTime;
	float2 padding;
}

[numthreads(8, 8, 1)]
void FillResultTextures(uint3 id : SV_DispatchThreadID)
{
	float2 _DxDz = DxDz[id.xy];
	float2 _DyDxz = DyDxz[id.xy];
	float2 _DyxDyz = DyxDyz[id.xy];
	float2 _DxxDzz = DxxDzz[id.xy];
	
	Displacement[id.xy] = float3(Lambda * _DxDz.x, _DyDxz.x, Lambda * _DxDz.y);
	Derivatives[id.xy] = float4(_DyxDyz, _DxxDzz * Lambda);
	float jacobian = (1 + Lambda * _DxxDzz.x) * (1 + Lambda * _DxxDzz.y) - Lambda * Lambda * _DyDxz.y * _DyDxz.y;
	Turbulence[id.xy] = Turbulence[id.xy].r + DeltaTime * 0.5 / max(jacobian, 0.5);
	Turbulence[id.xy] = min(jacobian, Turbulence[id.xy].r);
}




