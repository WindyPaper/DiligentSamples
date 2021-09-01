#ifndef _BASIC_STRUCTURES_FXH_
#define _BASIC_STRUCTURES_FXH_


#ifdef __cplusplus

#   ifndef BOOL
#      define BOOL int32_t // Do not use bool, because sizeof(bool)==1 !
#   endif

#   ifndef CHECK_STRUCT_ALIGNMENT
        // Note that defining empty macros causes GL shader compilation error on Mac, because
        // it does not allow standalone semicolons outside of main.
        // On the other hand, adding semicolon at the end of the macro definition causes gcc error.
#       define CHECK_STRUCT_ALIGNMENT(s) static_assert( sizeof(s) % 16 == 0, "sizeof(" #s ") is not multiple of 16" )
#   endif

#   ifndef DEFAULT_VALUE
#       define DEFAULT_VALUE(x) =x
#   endif

#else

#   ifndef BOOL
#       define BOOL bool
#   endif

#   ifndef DEFAULT_VALUE
#       define DEFAULT_VALUE(x)
#   endif

#endif

struct LightAttribs
{
    float4 f4Direction      DEFAULT_VALUE(float4(0, 0,-1, 0));
    float4 f4AmbientLight   DEFAULT_VALUE(float4(0, 0, 0, 0));
    float4 f4Intensity      DEFAULT_VALUE(float4(1, 1, 1, 1));
};
#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(LightAttribs);
#endif

struct CameraAttribs
{
    float4 f4Position;     // Camera world position
    float4 f4ViewportSize; // (width, height, 1/width, 1/height)

    float2 f2ViewportOrigin; // (min x, min y)
    float fNearPlaneZ; 
    float fFarPlaneZ; // fNearPlaneZ < fFarPlaneZ

#ifdef __cplusplus
    float4x4 mViewT;
    float4x4 mProjT;
    float4x4 mViewProjT;
    float4x4 mViewInvT;
    float4x4 mProjInvT;
    float4x4 mViewProjInvT;
#else
    matrix mView;
    matrix mProj;
    matrix mViewProj;
    matrix mViewInv;
    matrix mProjInv;
    matrix mViewProjInv;
#endif

    float4 f4ExtraData[5]; // Any appliation-specific data
    // Sizeof(CameraAttribs) == 256*2
};
#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(CameraAttribs);
#endif

#endif //_BASIC_STRUCTURES_FXH_
