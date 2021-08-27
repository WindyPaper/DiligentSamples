//almost from UE4 mothod

static const float PI = 3.141592653f;

float Pow4(float a)
{
	return a * a * a * a;
}

float Pow5(float a)
{
	return a * a * a * a * a;
}

struct BxDFContext
{
	float NoV;
	float NoL;
	float VoL;
	float NoH;
	float VoH;
};

void InitBrdf( inout BxDFContext Context, float3 N, float3 V, float3 L )
{
	Context.NoL = saturate(dot(N, L));
	Context.NoV = dot(N, V);
	Context.VoL = dot(V, L);
	float InvLenH = rsqrt( 2 + 2 * Context.VoL );
	Context.NoH = saturate( ( Context.NoL + Context.NoV ) * InvLenH );
	Context.VoH = saturate( InvLenH + InvLenH * Context.VoL );
}

float3 Diffuse_Lambert( float3 DiffuseColor )
{
	return DiffuseColor * (1 / PI);
}

// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX( float a2, float NoH )
{
	float d = ( NoH * a2 - NoH ) * NoH + 1;	// 2 mad
	return a2 / ( PI*d*d );					// 4 mul, 1 rcp
}

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox( float a2, float NoV, float NoL )
{
	float a = sqrt(a2);
	float Vis_SmithV = NoL * ( NoV * ( 1 - a ) + a );
	float Vis_SmithL = NoV * ( NoL * ( 1 - a ) + a );
	return 0.5 * rcp( Vis_SmithV + Vis_SmithL );
}

// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 F_Schlick( float3 SpecularColor, float VoH )
{
	float Fc = Pow5( 1 - VoH );					// 1 sub, 3 mul
	//return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad
	
	// Anything less than 2% is physically impossible and is instead considered to be shadowing
	return saturate( 50.0 * SpecularColor.g ) * Fc + (1 - Fc) * SpecularColor;	
}

float3 SpecularGGX( float Roughness, float3 SpecularColor, BxDFContext Context, float NoL)
{
	float a2 = Pow4( Roughness );
	
	// Generalized microfacet specular
	float D = D_GGX( a2, Context.NoH );
	float Vis = Vis_SmithJointApprox( a2, Context.NoV, NoL );
	float3 F = F_Schlick( SpecularColor, Context.VoH );

	return (D * Vis) * F;
}

// Note: Disney diffuse must be multiply by diffuseAlbedo / PI. This is done outside of this function.
float DisneyDiffuse(float NdotV, float NdotL, float LdotH, float perceptualRoughness)
{
    float fd90 = 0.5 + 2 * LdotH * LdotH * perceptualRoughness;
    // Two schlick fresnel term
    float lightScatter   = (1 + (fd90 - 1) * Pow5(1 - NdotL));
    float viewScatter    = (1 + (fd90 - 1) * Pow5(1 - NdotV));

    return lightScatter * viewScatter;
}

// Lambertian diffuse
// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
float3 LambertianDiffuse(float3 DiffuseColor)
{
    return DiffuseColor / PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
// (https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf), Equation 15
float3 SchlickReflection(float VdotH, float3 Reflectance0, float3 Reflectance90)
{
    return Reflectance0 + (Reflectance90 - Reflectance0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// Visibility = G(v,l,a) / (4 * (n,v) * (n,l))
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float SmithGGXVisibilityCorrelated(float NdotL, float NdotV, float AlphaRoughness)
{
    float a2 = AlphaRoughness * AlphaRoughness;

    float GGXV = NdotL * sqrt(max(NdotV * NdotV * (1.0 - a2) + a2, 1e-7));
    float GGXL = NdotV * sqrt(max(NdotL * NdotL * (1.0 - a2) + a2, 1e-7));

    return 0.5 / (GGXV + GGXL);
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float NormalDistribution_GGX(float NdotH, float AlphaRoughness)
{
    float a2 = AlphaRoughness * AlphaRoughness;
    float f = NdotH * NdotH * (a2 - 1.0)  + 1.0;
    return a2 / (PI * f * f);
}

struct SurfaceReflectanceInfo
{
    float  PerceptualRoughness;
    float3 Reflectance0;
    float3 Reflectance90;
    float3 DiffuseColor;
};

struct AngularInfo
{
    float NdotL;   // cos angle between normal and light direction
    float NdotV;   // cos angle between normal and view direction
    float NdotH;   // cos angle between normal and half vector
    float LdotH;   // cos angle between light direction and half vector
    float VdotH;   // cos angle between view direction and half vector
};

AngularInfo GetAngularInfo(float3 L, float3 Normal, float3 View)
{
    float3 n = normalize(Normal);       // Outward direction of surface point
    float3 v = normalize(View);         // Direction from surface point to camera
    float3 l = normalize(L); // Direction from surface point to light
    float3 h = normalize(l + v);        // Direction of the vector between l and v

    AngularInfo info;
    info.NdotL = saturate(dot(n, l));
    info.NdotV = saturate(dot(n, v));
    info.NdotH = saturate(dot(n, h));
    info.LdotH = saturate(dot(l, h));
    info.VdotH = saturate(dot(v, h));

    return info;
}

/// Calculates surface reflectance info

/// \param [in]  Workflow     - PBR workflow (PBR_WORKFLOW_SPECULAR_GLOSINESS or PBR_WORKFLOW_METALLIC_ROUGHNESS).
/// \param [in]  BaseColor    - Material base color.
SurfaceReflectanceInfo PBR_GetSurfaceReflectance(float3 BaseColor, float Roughness, float Metallic)
{
    SurfaceReflectanceInfo SrfInfo;

    float3 specularColor;

    float3 f0 = float3(0.04, 0.04, 0.04);

    
    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data
    SrfInfo.PerceptualRoughness = Roughness;

    SrfInfo.DiffuseColor  = BaseColor.rgb * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - Metallic);
    specularColor         = lerp(f0, BaseColor.rgb, Metallic);    

//#ifdef ALPHAMODE_OPAQUE
//    baseColor.a = 1.0;
//#endif
//
//#ifdef MATERIAL_UNLIT
//    gl_FragColor = float4(gammaCorrection(baseColor.rgb), baseColor.a);
//    return;
//#endif

    SrfInfo.PerceptualRoughness = clamp(SrfInfo.PerceptualRoughness, 0.0, 1.0);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    SrfInfo.Reflectance0  = specularColor.rgb;
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    SrfInfo.Reflectance90 = clamp(reflectance * 50.0, 0.0, 1.0) * float3(1.0, 1.0, 1.0);

    return SrfInfo;
}

void BRDF(in float3                 L, 
          in float3                 Normal,
          in float3                 View,
          in SurfaceReflectanceInfo SrfInfo,
          out float3                DiffuseContrib,
          out float3                SpecContrib,
          out float                 NdotL)
{
    AngularInfo angularInfo = GetAngularInfo(L, Normal, View);

    DiffuseContrib = float3(0.0, 0.0, 0.0);
    SpecContrib    = float3(0.0, 0.0, 0.0);
    NdotL          = angularInfo.NdotL;
    // If one of the dot products is larger than zero, no division by zero can happen. Avoids black borders.
    //if (angularInfo.NdotL > 0.0 || angularInfo.NdotV > 0.0)
    {
        //           D(h,a) * G(v,l,a) * F(v,h,f0)
        // f(v,l) = -------------------------------- = D(h,a) * Vis(v,l,a) * F(v,h,f0)
        //               4 * (n,v) * (n,l)
        // where
        //
        // Vis(v,l,a) = G(v,l,a) / (4 * (n,v) * (n,l))

        // It is not a mistake that AlphaRoughness = PerceptualRoughness ^ 2 and that
        // SmithGGXVisibilityCorrelated and NormalDistribution_GGX then use a2 = AlphaRoughness ^ 2.
        // See eq. 3 in https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
        float AlphaRoughness = SrfInfo.PerceptualRoughness * SrfInfo.PerceptualRoughness;
        float  D   = NormalDistribution_GGX(angularInfo.NdotH, AlphaRoughness);
        float  Vis = SmithGGXVisibilityCorrelated(angularInfo.NdotL, angularInfo.NdotV, AlphaRoughness);
        float3 F   = SchlickReflection(angularInfo.VdotH, SrfInfo.Reflectance0, SrfInfo.Reflectance90);

        DiffuseContrib = (1.0 - F) * LambertianDiffuse(SrfInfo.DiffuseColor);
        SpecContrib    = F * Vis * D;
    }
}

float3 PBR_ApplyDirectionalLight(float3 lightDir, float3 lightColor, SurfaceReflectanceInfo srfInfo, float3 normal, float3 view)
{
    float3 diffuseContrib, specContrib;
    float  NdotL;
    BRDF(lightDir, normal, view, srfInfo, diffuseContrib, specContrib, NdotL);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    float3 shade = (diffuseContrib + specContrib) * NdotL;
    return lightColor * shade;
}