
struct ParticleAttribs
{
    float2 f2Pos;
    float2 f2NewPos;

    float2 f2Speed;
    float2 f2NewSpeed;

    float  fSize;
    int    iNumCollisions;
};

struct GlobalConstants
{
	float4x4 L2C;

    uint   uiNumParticles;
    float  fDeltaTime;
    float  fDummy0;
    float  fDummy1;

    float2 f2Scale;
    int2   i2ParticleGridSize;
};
