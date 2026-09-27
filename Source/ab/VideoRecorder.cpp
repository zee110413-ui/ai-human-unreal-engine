#include "VideoRecorder.h"
#include "FrameGrabber.h"
#include "Engine/GameEngine.h"
#include "Slate/SceneViewport.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Modules/ModuleManager.h"

FClipRecorder::FClipRecorder()
{
}

FClipRecorder::~FClipRecorder()
{
    Stop();
}

bool FClipRecorder::Start(const FString& InFolder)
{
    UGameEngine* Game = Cast<UGameEngine>(GEngine);
    if (!Game || !Game->SceneViewport.IsValid())
    {
        return false;
    }
    Wrappers = &FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    Folder = InFolder;
    const FIntPoint Size = Game->SceneViewport->GetSize();
    if (Size.X <= 0 || Size.Y <= 0)
    {
        return false;
    }
    Grabber = MakeUnique<FFrameGrabber>(Game->SceneViewport.ToSharedRef(), Size, PF_B8G8R8A8, 4);
    Grabber->StartCapturingFrames();
    return true;
}

void FClipRecorder::Capture()
{
    if (Grabber)
    {
        Grabber->CaptureThisFrame(FFramePayloadPtr());
    }
}

void FClipRecorder::Flush(bool bWait)
{
    if (!Grabber || !Wrappers)
    {
        return;
    }
    TArray<FCapturedFrameData> Frames = Grabber->GetCapturedFrames();
    for (FCapturedFrameData& Frame : Frames)
    {
        const FString Path = Folder / FString::Printf(TEXT("frame_%05d.jpg"), Next++);
        IImageWrapperModule* Module = Wrappers;
        Pending.Add(Async(EAsyncExecution::ThreadPool,
            [Module, Pixels = MoveTemp(Frame.ColorBuffer), Size = Frame.BufferSize, Path]() mutable
            {
                if (Pixels.Num() != Size.X * Size.Y)
                {
                    return;
                }
                for (FColor& Colour : Pixels)
                {
                    Colour.A = 255;
                }
                TSharedPtr<IImageWrapper> Writer = Module->CreateImageWrapper(EImageFormat::JPEG);
                if (Writer.IsValid() && Writer->SetRaw(Pixels.GetData(), Pixels.Num() * 4, Size.X, Size.Y, ERGBFormat::BGRA, 8))
                {
                    FFileHelper::SaveArrayToFile(Writer->GetCompressed(88), *Path);
                }
            }));
    }
    if (bWait)
    {
        for (TFuture<void>& Task : Pending)
        {
            Task.Wait();
        }
        Pending.Reset();
    }
    else
    {
        Pending.RemoveAll([](const TFuture<void>& Task) { return Task.IsReady(); });
    }
}

void FClipRecorder::Stop()
{
    if (!Grabber)
    {
        return;
    }
    Grabber->StopCapturingFrames();
    Flush(true);
    Grabber->Shutdown();
    Grabber.Reset();
}
