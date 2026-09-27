#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"

class FFrameGrabber;
class IImageWrapperModule;

class AB_API FClipRecorder
{
public:
    FClipRecorder();
    ~FClipRecorder();

    bool Start(const FString& InFolder);
    void Capture();
    void Flush(bool bWait);
    void Stop();
    int32 Captured() const { return Next; }

private:
    TUniquePtr<FFrameGrabber> Grabber;
    IImageWrapperModule* Wrappers = nullptr;
    FString Folder;
    int32 Next = 0;
    TArray<TFuture<void>> Pending;
};
