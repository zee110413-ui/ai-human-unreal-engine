#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HumanMovementComponent.generated.h"

UCLASS()
class AB_API UHumanMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    FVector GetIntent() const { return Intent; }
    void DriveByBody(const FVector& Drive);
    void ReleaseBody();
    bool IsBodyDriven() const { return bBodyDrives; }
    FVector GetDrivenVelocity() const { return BodyVelocity; }

protected:
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
    virtual void PhysicsRotation(float DeltaTime) override;

private:
    FVector Intent = FVector::ZeroVector;
    FVector BodyVelocity = FVector::ZeroVector;
    bool bBodyDrives = false;
};
