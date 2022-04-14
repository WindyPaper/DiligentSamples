#include "PCGPoint.h"
#include "PoissonDisk/PoissonDiskSampling.h"

#include <fstream>


//Diligent::PCGPoint::PCGPoint(float radius, float2 min_c, float2 max_c, std::uint32_t seed /*= 0*/)
//{
//	GeneratePoints(radius, min_c, max_c, seed);
//}

Diligent::PCGPoint::PCGPoint()
{

}

void Diligent::PCGPoint::ReadPoints(const uint Layer)
{
	char DatName[128];
	sprintf(DatName, "./PoissonLayer%d.dat", Layer);

	std::ifstream rf(DatName, std::ios::out | std::ios::binary);
	int pointNum;
	rf.read((char*)&pointNum, sizeof(int));
	mPoints.resize(pointNum);
	rf.read((char*)&mPoints[0][0], sizeof(float) * 2 * pointNum);
	rf.close();
}

Diligent::PCGPoint::~PCGPoint()
{

}

void Diligent::PCGPoint::GeneratePoints(float radius, float2 min_c, float2 max_c, std::uint32_t seed)
{
	auto kXMin = std::array<float, 2>{ {min_c.x, min_c.y} };
	auto kXMax = std::array<float, 2>{ {max_c.x, max_c.y} };

	mPoints = thinks::PoissonDiskSampling(radius, kXMin, kXMax);
}
