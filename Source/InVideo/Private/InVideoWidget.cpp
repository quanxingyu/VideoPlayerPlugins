// Fill out your copyright notice in the Description page of Project Settings.


#include "InVideoWidget.h"
#include "Async/Async.h"
#include "Rendering/Texture2DResource.h"

void UInVideoWidget::NativeConstruct()
{
	UE_LOG(LogTemp, Log, TEXT("UInVideoWidget NativeConstruct"));
	Super::NativeConstruct();
}
void UInVideoWidget::NativeDestruct()
{
	UE_LOG(LogTemp, Log, TEXT("UInVideoWidget NativeDestruct"));
	StopPlay();
	Super::NativeDestruct();
}


void UInVideoWidget::StartPlay(const FString VideoURL, FDelegatePlayFailed Failed, FDelegateFirstFrame FirstFrame,const bool RealMode , const int Fps)
{
	StopPlay();
	m_VideoPlayPtr = MakeUnique<VideoPlay>();
	m_VideoPlayPtr->StartPlay(VideoURL, Failed, FirstFrame, RealMode, Fps,this);
}
void UInVideoWidget::StopPlay()
{
	if (m_VideoPlayPtr.Get() == nullptr)
	{
		return;
	}
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [ptr = MoveTemp(m_VideoPlayPtr)]()
		{
		 ptr->StopPlay();
		});
}

void UInVideoWidget::SetPlayRate(float Rate)
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->SetPlayRate(Rate);
	}
}

void UInVideoWidget::SetReverse(bool bReverse)
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->SetReverse(bReverse);
	}
}

void UInVideoWidget::SetVideoResolution(const FVector2D& NewResolution)
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->SetResolution(NewResolution);
	}
}

void UInVideoWidget::LoadConfig(const FString& ConfigName)
{
	// 参数验证
	if (ConfigName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Config name is empty, using default resolution"));
		SetVideoResolution(FVector2D(0, 0));
		return;
	}

	// 构建配置文件路径
	const FString ConfigPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() + ConfigName);

	// 检查文件是否存在
	if (!FPaths::FileExists(ConfigPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Config file not found at %s, using default resolution"), *ConfigPath);
		SetVideoResolution(FVector2D(0, 0));
		return;
	}

	// 读取配置文件
	FConfigFile Config;
	Config.Read(ConfigPath);

	// 读取分辨率配置
	int32 Width = 0;
	int32 Height = 0;
	const bool bHasWidth = Config.GetInt(TEXT("Video"), TEXT("Width"), Width);
	const bool bHasHeight = Config.GetInt(TEXT("Video"), TEXT("Height"), Height);

	// 验证配置值
	if (!bHasWidth || !bHasHeight || Width <= 0 || Height <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid resolution in config file: %dx%d"), Width, Height);
		SetVideoResolution(FVector2D(0, 0));
		return;
	}

	// 设置有效的分辨率
	SetVideoResolution(FVector2D(Width, Height));
	UE_LOG(LogTemp, Log, TEXT("Successfully set resolution to %dx%d"), Width, Height);
}

void UInVideoWidget::ContinuePlay(int32 FrameIndex)
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->ContinuePlay(FrameIndex);
	}
}

void UInVideoWidget::PausePlay()
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->PausePlay();
	}
}

void UInVideoWidget::ResumePlay()
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->ResumePlay();
	}
}



void VideoPlay::StartPlay(const FString VideoURL, FDelegatePlayFailed Failed, FDelegateFirstFrame FirstFrame, const bool RealMode, const int Fps, UInVideoWidget* widget)
{
	StopPlay();
	m_widget = widget;
	m_Stopping = false;
	m_VideoURL = VideoURL;
	if (FPaths::IsRelative(m_VideoURL))
	{
		m_VideoURL = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / m_VideoURL);
	}
	m_RealMode = RealMode;
	m_Fps = Fps;
	m_UpdateTime = 1000 / m_Fps;
	m_Failed = Failed;
	m_FirstFrame = FirstFrame;
	m_BFirstFrame = false;
	UE_LOG(LogTemp, Log, TEXT("VideoPlay StartPlay Enter"));
	m_Thread = FRunnableThread::Create(this, TEXT("Video Thread"));
	UE_LOG(LogTemp, Log, TEXT("VideoPlay StartPlay END"));
}
void VideoPlay::StopPlay()
{
	UE_LOG(LogTemp, Log, TEXT("VideoPlay StopPlay Enter"));
	m_Stopping = true;
	if (nullptr != m_Thread)
	{
		m_Thread->Kill();
		delete m_Thread;
		m_Thread = nullptr;
	}
	if (nullptr != m_WrapOpenCv)
	{
		if (m_WrapOpenCv->m_Stream.isOpened())
		{
			m_WrapOpenCv->m_Stream.release();
		}
		delete m_WrapOpenCv;
		m_WrapOpenCv = nullptr;
	}
	if (nullptr != m_VideoUpdateTextureRegion)
	{
		delete m_VideoUpdateTextureRegion;
		m_VideoUpdateTextureRegion = nullptr;
	}
	AsyncTask(ENamedThreads::GameThread, [vt = VideoTexture]()
		{
			if (vt->IsValidLowLevel()) 
			{
				vt->RemoveFromRoot();
			}
		});
	m_VideoSize = FVector2D(0, 0);
	UE_LOG(LogTemp, Log, TEXT("UInVideoWidget StopPlay END"));
}
void VideoPlay::SetPlayRate(float Rate)
{
	m_PlayRate = FMath::Max(0.1f, Rate);
}
void VideoPlay::SetReverse(bool bReverse)
{
	m_bReverse = bReverse;
}

void VideoPlay::SetResolution(const FVector2D& NewResolution)
{
	if (NewResolution.X > 0 && NewResolution.Y > 0)
	{
		m_bCustomResolution = true;
		m_TargetResolution = NewResolution;
	}
	else
	{
		m_bCustomResolution = false;
	}
}

void VideoPlay::ContinuePlay(int32 FrameIndex)
{
	if (m_WrapOpenCv && m_WrapOpenCv->m_Stream.isOpened())
	{
		if (FrameIndex >= 0)
		{
			m_CurrentFrameIndex = FrameIndex;
		}

		// 设置视频播放位置
		m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);

		// 如果线程已停止,重新启动
		if (m_Stopping)
		{
			m_Stopping = false;
			m_Thread = FRunnableThread::Create(this, TEXT("Video Thread"));
		}
	}
}
void VideoPlay::PausePlay()
{
	m_bPaused = true;

}
void VideoPlay::ResumePlay()
{
	m_bPaused = false;
}
bool VideoPlay::Init()
{
	return true;
}
uint32 VideoPlay::Run()
{
	UE_LOG(LogTemp, Log, TEXT("VideoPlay open Enter"));
	if (nullptr == m_WrapOpenCv)
	{
		m_WrapOpenCv = new WrapOpenCv();
	}
	if (false == m_WrapOpenCv->m_Stream.open(TCHAR_TO_UTF8(*m_VideoURL)))
	{
		UE_LOG(LogTemp, Error, TEXT("VideoPlay open url=%s"), *m_VideoURL);
		NotifyFailed();
		return -1;
	}

	// 获取总帧数
	m_TotalFrames = m_WrapOpenCv->m_Stream.get(cv::CAP_PROP_FRAME_COUNT);
	UE_LOG(LogTemp, Log, TEXT("VideoPlay open END, Total Frames: %d"), m_TotalFrames);

	UE_LOG(LogTemp, Log, TEXT("VideoPlay Run Enter"));
	while (false == m_Stopping)
	{
		if (m_bPaused)
		{
			FPlatformProcess::Sleep(0.0001f);
			continue;
		}

		if (false == m_WrapOpenCv->m_Stream.isOpened())
		{
			UE_LOG(LogTemp, Error, TEXT("VideoPlay Run isOpened"));
			NotifyFailed();
			return -1;
		}

		if (false == m_RealMode)
		{
			auto DataNow = FDateTime::Now();
			if ((DataNow - m_LastReadTime).GetTotalMilliseconds() < m_UpdateTime/ m_PlayRate)
			{
				FPlatformProcess::Sleep(0.0001);
				continue;
			}
			m_LastReadTime = DataNow;

			// 根据方向更新帧索引
			if (m_bReverse)
			{
				m_CurrentFrameIndex -= 3*m_PlayRate;
				if (m_CurrentFrameIndex < 0)
				{
					m_CurrentFrameIndex = m_TotalFrames - 1;
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
				}
			}
			else
			{
				m_CurrentFrameIndex += m_PlayRate;
				if (m_CurrentFrameIndex >= m_TotalFrames)
				{
					m_CurrentFrameIndex = 0;
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, 0);
				}
			}

			// 只在需要时设置视频位置
			if ( m_CurrentFrameIndex > 0)
			{
				m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
			}

			if (true == m_WrapOpenCv->m_Stream.read(m_WrapOpenCv->m_Frame))
			{
				NotifyFirstFrame();
				UpdateTexture();
			}
			else
			{
				// 读取失败时重试
				if (m_bReverse)
				{
					m_CurrentFrameIndex = m_TotalFrames - 1;
				}
				else
				{
					m_CurrentFrameIndex = 0;
				}
				m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
				continue;
			}
		}
		else
		{
			// 实时模式的处理
			FPlatformProcess::Sleep(m_SleepSecond / m_PlayRate);

			if (true == m_WrapOpenCv->m_Stream.read(m_WrapOpenCv->m_Frame))
			{
				NotifyFirstFrame();
				UpdateTexture();
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("VideoPlay Run END"));
	return 0;
}
void VideoPlay::Exit()
{

}
void VideoPlay::Stop()
{

}
void VideoPlay::NotifyFailed()
{
	AsyncTask(ENamedThreads::GameThread, [Failed= m_Failed]()
		{
			if (Failed.IsBound())
				Failed.Execute();
		});
}
void VideoPlay::NotifyFirstFrame()
{
	if (m_BFirstFrame)
	{
		return;
	}
	m_BFirstFrame = true;
	AsyncTask(ENamedThreads::GameThread, [FirstFrame = m_FirstFrame]()
		{
			if (FirstFrame.IsBound())
				FirstFrame.Execute();
		});
}
void VideoPlay::UpdateTexture()
{
	// 1. 得到最终用于渲染的 Mat —— resizedFrame
	cv::Mat resizedFrame;
	if (m_bCustomResolution)
	{
		// 如果需要自定义分辨率，就对原始帧做resize
		cv::resize(m_WrapOpenCv->m_Frame, resizedFrame,
			cv::Size(m_TargetResolution.X, m_TargetResolution.Y));
	}
	else
	{
		// 如果不需要自定义分辨率，就直接使用原始帧
		resizedFrame = m_WrapOpenCv->m_Frame;
	}

	// 2. 用 resizedFrame 的实际大小后续全部使用
	const int32 NewWidth = resizedFrame.cols;
	const int32 NewHeight = resizedFrame.rows;

	// 3. 判断是否需要重建纹理资源
	if (VideoTexture == nullptr
		|| m_VideoSize.X != NewWidth
		|| m_VideoSize.Y != NewHeight)
	{
		// 更新记录的当前视频大小
		m_VideoSize = FVector2D(NewWidth, NewHeight);

		// 在GameThread里创建或重置纹理
		FEvent* SyncEvent = FGenericPlatformProcess::GetSynchEventFromPool(false);
		AsyncTask(ENamedThreads::GameThread, [this, SyncEvent]()
			{
				// 创建Transient纹理
				VideoTexture = UTexture2D::CreateTransient(m_VideoSize.X, m_VideoSize.Y);
				if (VideoTexture)
				{
					VideoTexture->UpdateResource();
					VideoTexture->AddToRoot();  // 防止被GC
				}
				// 记录资源指针
				m_Texture2DResource = (FTexture2DResource*)VideoTexture->GetResource();
				SyncEvent->Trigger();
			});
		SyncEvent->Wait();
		FGenericPlatformProcess::ReturnSynchEventToPool(SyncEvent);

		// 重建 UpdateTextureRegion
		if (m_VideoUpdateTextureRegion)
		{
			delete m_VideoUpdateTextureRegion;
			m_VideoUpdateTextureRegion = nullptr;
		}
		m_VideoUpdateTextureRegion = new FUpdateTextureRegion2D(
			0, 0, 0, 0,
			NewWidth, NewHeight
		);

		// 分配 Data 数组
		Data.Init(FColor(0, 0, 0, 255), NewWidth * NewHeight);
	}

	// 如果依然没有 Texture，说明可能初始化失败，直接返回
	if (!VideoTexture)
	{
		return;
	}

	// 4. 填充像素数据 (BGR => FColor)
	for (int y = 0; y < NewHeight; y++)
	{
		for (int x = 0; x < NewWidth; x++)
		{
			int32 i = x + y * NewWidth;
			Data[i].B = resizedFrame.data[i * 3 + 0];
			Data[i].G = resizedFrame.data[i * 3 + 1];
			Data[i].R = resizedFrame.data[i * 3 + 2];
			// Data[i].A = 255;  // 如果需要手动设置透明度
		}
	}

	// 5. 更新纹理区域
	UpdateTextureRegions(VideoTexture, 0, 1, m_VideoUpdateTextureRegion,
		(uint32)(4 * NewWidth), (uint32)4, (uint8*)Data.GetData(), false);

	// 6. 在Game Thread中设置 UImage 的 Brush
	AsyncTask(ENamedThreads::GameThread, [vt = VideoTexture, widget = m_widget]()
		{
			if (!widget.IsValid() || !vt->IsValidLowLevel() || !widget->ImageVideo)
			{
				return;
			}
			widget->ImageVideo->SetBrushFromTexture(vt);
		});
}
void VideoPlay::UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData)
{
	if (m_Texture2DResource)
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			uint32 NumRegions;
			FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};
		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = m_Texture2DResource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = new FUpdateTextureRegion2D(*Regions);
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_RENDER_COMMAND(UpdateTextureRegionsData)([RegionData, bFreeData](FRHICommandListImmediate& RHICmdList) {

			for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
			{
				int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
				if (RegionData->MipIndex >= CurrentFirstMip)
				{
					RHIUpdateTexture2D(
						RegionData->Texture2DResource->GetTexture2DRHI(),
						RegionData->MipIndex - CurrentFirstMip,
						RegionData->Regions[RegionIndex],
						RegionData->SrcPitch,
						RegionData->SrcData
						+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
						+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
					);
				}
			}
			if (bFreeData)
			{
				//FMemory::Free(RegionData->Regions);
				//FMemory::Free(RegionData->SrcData);

			}
			delete RegionData->Regions;
			delete RegionData;
			});
	}
}