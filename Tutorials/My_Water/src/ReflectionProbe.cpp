#include "ReflectionProbe.h"

Diligent::ReflectionProbe::ReflectionProbe(const float3& pos)
{
	float x = pos.x;
	float y = pos.y;
	float z = pos.z;

	float3 targets[6] =
	{
		float3(x + 1.0f, y, z), // +X
		float3(x - 1.0f, y, z), // -X
		float3(x, y + 1.0f, z), // +Y
		float3(x, y - 1.0f, z), // -Y
		float3(x, y, z + 1.0f), // +Z
		float3(x, y, z - 1.0f)  // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		m_Cameras[i].SetPos(pos);
		m_Cameras[i].SetLookAt(targets[i]);

		float NearPlane = 0.1f;
		float FarPlane = 100000.f;
		float AspectRatio = 1.0f;
		m_Cameras[i].SetProjAttribs(NearPlane, FarPlane, AspectRatio, 3.14f / 2.f,
			SURFACE_TRANSFORM_IDENTITY, false);

		m_Cameras[i].InvalidUpdate();
	}
}

Diligent::ReflectionProbe::~ReflectionProbe()
{

}

Diligent::FirstPersonCamera * Diligent::ReflectionProbe::GetCameras()
{
	return &m_Cameras[0];
}
