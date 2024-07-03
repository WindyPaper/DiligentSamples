cbuffer SimpleObjConstants
{
    float4x4 g_WorldViewProj;
    float4x4 g_ViewMat;
    float4x4 g_ProjMat;
    float4 g_CamPos;
    float4 g_CamForward;
    float4 g_BakeDirAndNum;

    float TestPlaneOffsetY;
    float BakeHeightScale;
    float BakeTexTiling;
    float padding;

    float4 bbox_min;
    float4 bbox_max;
};