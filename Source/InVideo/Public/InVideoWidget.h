// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HAL/Runnable.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"

#include "PreOpenCVHeaders.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include "PostOpenCVHeaders.h"

#include "InVideoWidget.generated.h"

DECLARE_DYNAMIC_DELEGATE(FDelegatePlaySucceeded);
DECLARE_DYNAMIC_DELEGATE(FDelegatePlayFailed);
DECLARE_DYNAMIC_DELEGATE(FDelegateFirstFrame);


class VideoPlay :public FRunnable
{
public:
	void StartPlay(const FString VideoURL, FDelegatePlayFailed Failed, FDelegateFirstFrame FirstFrame,
		const bool RealMode = true, const int Fps = 25, UInVideoWidget* widget=nullptr);
	void StopPlay();

	void SetPlayRate(float Rate);
	void SetReverse(bool bReverse);
	void SetResolution(const FVector2D& NewResolution);
	void ContinuePlay(int32 FrameIndex = -1);
	void PausePlay();
	void ResumePlay();
public:
	bool Init() override;
	uint32 Run() override;
	void Stop() override;
	void Exit() override;
private:
	void UpdateTexture();
	void UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData);
	void NotifyFailed();
	void NotifyFirstFrame();
public:
	UTexture2D* VideoTexture = nullptr;
	TWeakObjectPtr<UInVideoWidget> m_widget = nullptr;
private:
	FRunnableThread* m_Thread = nullptr;
	TAtomic<bool> m_Stopping = false;
	FString m_VideoURL;
	float m_UpdateTime = 20;
	int m_Fps = 25;
	FVector2D m_TargetResolution = FVector2D(0, 0);
	float m_PlayRate = 1.0f;
	bool m_bReverse = false;
	bool m_bPaused = false;
	bool m_bCustomResolution = false;
	int32 m_CurrentFrameIndex = 0;
	int32 m_TotalFrames = 0;
	bool m_RealMode = true;
	float m_SleepSecond = 1 / 50;
	FDateTime m_LastReadTime = FDateTime::Now();

	FDelegatePlayFailed m_Failed;
	FDelegateFirstFrame m_FirstFrame;
	bool m_BFirstFrame = false;
	class WrapOpenCv
	{
	public:
		cv::VideoCapture m_Stream;
		cv::Mat m_Frame;
	};
	WrapOpenCv* m_WrapOpenCv = nullptr;

	FVector2D m_VideoSize = FVector2D(0, 0);
	FUpdateTextureRegion2D* m_VideoUpdateTextureRegion = nullptr;
	FTexture2DResource* m_Texture2DResource = nullptr;
	TArray64<FColor> Data;
};

UCLASS()
class INVIDEO_API UInVideoWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:


	UFUNCTION(BlueprintCallable, Category = "InVideo")
    void StartPlay(const FString VideoURL, FDelegatePlayFailed Failed, FDelegateFirstFrame FirstFrame,
		const bool RealMode = true,const int Fps=25);

	UFUNCTION(BlueprintCallable, Category = "InVideo")
	void StopPlay();

	UFUNCTION(BlueprintCallable,Category = "InVideo")
	void SetPlayRate(float Rate);

	UFUNCTION(BlueprintCallable,Category = "InVideo")
	void SetReverse(bool bReverse);

	UFUNCTION(BlueprintCallable,Category = "InVideo")
	void SetVideoResolution(const FVector2D& NewResolution);

	UFUNCTION(BlueprintCallable,Category = "InVideo")
	void LoadConfig(const FString& ConfigName);

	//已经废弃
	UFUNCTION(BlueprintCallable,Category = "InVideo")
	void ContinuePlay(int32 FrameIndex = -1);

	UFUNCTION(BlueprintCallable,Category = "InVideo")
	void PausePlay();

	UFUNCTION(BlueprintCallable, Category = "InVideo")
	void ResumePlay();

	UPROPERTY(BlueprintReadWrite, Meta = (BindWidget),Category = "InVideo")
	UImage* ImageVideo;
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	TUniquePtr<VideoPlay> m_VideoPlayPtr;
};

