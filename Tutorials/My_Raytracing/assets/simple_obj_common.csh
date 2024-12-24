cbuffer SimpleObjConstants
{
    float4x4 g_WorldViewProj;
    float4x4 g_ViewMat;
    float4x4 g_ProjMat;
    float4x4 g_ViewProjMatInv;    
    float4 g_CamPos;
    float4 g_CamForward;
    float4 g_BakeDirAndNum;

    float TestPlaneOffsetY;
    float BakeHeightScale;
    float BakeTexTiling;
    float FlowIntensity;

    float4 bbox_min;
    float4 bbox_max;
};

float3x3 Inverse3x3(float3x3 m)
{
    float det = 
    m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) -
    m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
    m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

  float invdet = 1.0f / det;

  float3x3 res; // inverse of matrix m
  res[0][0] = (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * invdet;
  res[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invdet;
  res[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invdet;
  res[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invdet;
  res[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invdet;
  res[1][2] = (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * invdet;
  res[2][0] = (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * invdet;
  res[2][1] = (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * invdet;
  res[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * invdet;
  
  return res;
}