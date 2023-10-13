// Fill out your copyright notice in the Description page of Project Settings.


#include "MyPlayerController.h"
#include "Engine/EngineTypes.h"

bool AMyPlayerController::GetHitResultWithFaceIndexUnderCursorByChannel(ETraceTypeQuery TraceChannel, FHitResult& HitResult) const
{
    ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
    bool bHit = false;
    if (LocalPlayer && LocalPlayer->ViewportClient)
    {
        FVector2D MousePosition;
        if (LocalPlayer->ViewportClient->GetMousePosition(MousePosition))
        {
            //必须设置以下两个选项为true才能在HitResult中返回FaceIndex
            FCollisionQueryParams CollisionQueryParams;
            CollisionQueryParams.bTraceComplex = true;
            CollisionQueryParams.bReturnFaceIndex = true;
            bHit = GetHitResultAtScreenPosition(MousePosition, UEngineTypes::ConvertToCollisionChannel(TraceChannel), CollisionQueryParams, HitResult);
        }
    }

    if (!bHit)	//If there was no hit we reset the results. This is redundant but helps Blueprint users
    {
        HitResult = FHitResult();
    }

    return bHit;
}
