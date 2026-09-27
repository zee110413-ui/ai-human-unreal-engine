#include "HumanMovementComponent.h"

void UHumanMovementComponent::DriveByBody(const FVector& Drive)
{
    bBodyDrives = true;
    BodyVelocity = FVector(Drive.X, Drive.Y, 0.0f);
}

void UHumanMovementComponent::ReleaseBody()
{
    bBodyDrives = false;
    BodyVelocity = FVector::ZeroVector;
}

void UHumanMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    if (bHasRequestedVelocity)
    {
        Intent = RequestedVelocity;
    }
    else if (!Acceleration.IsNearlyZero())
    {
        Intent = Acceleration.GetSafeNormal() * GetMaxSpeed() * FMath::Clamp(AnalogInputModifier, 0.0f, 1.0f);
    }
    else
    {
        Intent = FVector::ZeroVector;
    }
    Intent.Z = 0.0f;

    if (bBodyDrives && IsMovingOnGround())
    {
        bHasRequestedVelocity = false;
        Velocity.X = BodyVelocity.X;
        Velocity.Y = BodyVelocity.Y;
        return;
    }
    Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UHumanMovementComponent::PhysicsRotation(float DeltaTime)
{
    if (bBodyDrives)
    {
        return;
    }
    Super::PhysicsRotation(DeltaTime);
}
