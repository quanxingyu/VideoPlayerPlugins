// Fill out your copyright notice in the Description page of Project Settings.


#include "InVideoWidget.h"
#include "MDOverlayWidget.h"
#include "MDOverlayWidget.h"
#include "MDItemWidget.h"
#include "MDModelDisplayActor.h"
#include "MDModelDisplayConfig.h"
#include "MDModelDisplayDragger.h"
#include "MDModelDisplayUtils.h"
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

void UInVideoWidget::BindOnFirstPlayCompleted(FDelegateFirstPlayCompleted Delegate)
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->BindFirstPlayCompletedDelegate(Delegate);
	}
}

void UInVideoWidget::BindOnVideoFileNotFound(FDelegateVideoFileNotFound Delegate)
{

	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->BindVideoFileNotFoundDelegate(Delegate);
	}
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

void UInVideoWidget::LoadVideoURLFromProfile(FString PlayCase)
{
	if (m_VideoPlayPtr.IsValid())
	{
		m_VideoPlayPtr->LoadVideoURLFromProfile(PlayCase);
	}
}





void VideoPlay::StartPlay(const FString VideoURL, FDelegatePlayFailed Failed, FDelegateFirstFrame FirstFrame, const bool RealMode, const int Fps, UInVideoWidget* widget)
{
	
	StopPlay();
	m_widget = widget;
	m_Stopping = false;
	LoadVideoURLFromProfile(VideoURL);
	UE_LOG(LogTemp, Warning, TEXT("VideoPlay::LoadVideoURLFromProfile - URL Exist!Start to play in %s"),*m_VideoURL);
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
void VideoPlay::BindFirstPlayCompletedDelegate(FDelegateFirstPlayCompleted Delegate)
{
	m_FirstPlayCompleted = Delegate;
}
void VideoPlay::BindVideoFileNotFoundDelegate(FDelegateVideoFileNotFound Delegate)
{
	m_VideoFileNotFound = Delegate;
}
void VideoPlay::NotifyFirstPlayCompleted()
{
	AsyncTask(ENamedThreads::GameThread, [FirstPlayCompleted = m_FirstPlayCompleted]()
		{
			if (FirstPlayCompleted.IsBound())
				FirstPlayCompleted.Execute();
		});
}
void VideoPlay::NotifyVideoFileNotFound()
{
	AsyncTask(ENamedThreads::GameThread, [VideoFileNotFound = m_VideoFileNotFound]()
		{
			if (VideoFileNotFound.IsBound())
				VideoFileNotFound.Execute();
		});
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

void VideoPlay::LoadVideoURLFromProfile(FString PlayCase)
{
	if (IsValid(m_widget->OwnerOverlay)&&m_widget->OwnerOverlay->ModelDisplayData.Num()>0)
	{
		FString Path = FPaths::Combine(UMDModelDisplayUtils::GetProjectDirectory(), TEXT("Showcase"));
		FPaths::NormalizeDirectoryName(Path);
		FString MP4Name = m_widget->OwnerOverlay->ModelDisplayData[m_widget->OwnerOverlay->CurrentModelIndex].MP4Name;
		FString PreviewMP4Name = m_widget->OwnerOverlay->ModelDisplayData[m_widget->OwnerOverlay->CurrentModelIndex].PreviewMP4;
		FString IntroMP4 = m_widget->OwnerOverlay->ModelDisplayData[m_widget->OwnerOverlay->CurrentModelIndex].IntroMP4;

		IFileManager& FileManager = IFileManager::Get();

		TArray<FString> AllDirectories;
		FileManager.FindFilesRecursive(AllDirectories, *Path, TEXT("*"), false, true);

		FString m_MP4URL = FPaths::Combine(AllDirectories[m_widget->OwnerOverlay->CurrentModelIndex], MP4Name);
		FString m_PreviewMP4URL = FPaths::Combine(AllDirectories[m_widget->OwnerOverlay->CurrentModelIndex], PreviewMP4Name);
		FString m_IntroMP4URL = FPaths::Combine(AllDirectories[m_widget->OwnerOverlay->CurrentModelIndex], IntroMP4);
		UE_LOG(LogTemp, Warning, TEXT("VideoPlay::LoadVideoURLFromProfile - Get Data!.MP4URL is %s"),*m_MP4URL);
		UE_LOG(LogTemp, Warning, TEXT("VideoPlay::LoadVideoURLFromProfile - Get Data!.PreviewMP4Name is %s"), *m_PreviewMP4URL);
		UE_LOG(LogTemp, Warning, TEXT("VideoPlay::LoadVideoURLFromProfile - Get Data!.IntroMP4 is %s"), *m_IntroMP4URL);

		//if (FPaths::FileExists(m_MP4URL)|| FPaths::FileExists(m_PreviewMP4URL)||FPaths::FileExists(m_IntroMP4URL))
		//{
		//	if (PlayCase == "1")m_VideoURL = std::move(m_MP4URL);
		//	if (PlayCase == "2")m_VideoURL = std::move(m_IntroMP4URL);
		//	if (PlayCase == "3")m_VideoURL = std::move(m_MP4URL);
		//	
		//	UE_LOG(LogTemp, Warning, TEXT("VideoPlay::LoadVideoURLFromProfile - URL Exist!Start to play"));
		//}
		//else
		//{
		//	UE_LOG(LogTemp, Error, TEXT("VideoPlay::LoadVideoURLFromProfile - ERROR!"));

		//}

		if (PlayCase == TEXT("1"))
		{
			if(FPaths::FileExists(m_PreviewMP4URL))m_VideoURL=std::move(m_PreviewMP4URL);
			else
			{
				UE_LOG(LogTemp, Error, TEXT("VideoPlay::LoadVideoURLFromProfile - ERROR!"));
				NotifyVideoFileNotFound();
			}
		}
		if (PlayCase == TEXT("2"))
		{
			if (FPaths::FileExists(m_IntroMP4URL))m_VideoURL = std::move(m_IntroMP4URL);
			else
			{
				UE_LOG(LogTemp, Error, TEXT("VideoPlay::LoadVideoURLFromProfile - ERROR!"));
				NotifyVideoFileNotFound();
			}
		}
		if (PlayCase == TEXT("3"))
		{
			if (FPaths::FileExists(m_MP4URL))m_VideoURL = std::move(m_MP4URL);
			else
			{
				UE_LOG(LogTemp, Error, TEXT("VideoPlay::LoadVideoURLFromProfile - ERROR!"));
				NotifyVideoFileNotFound();
			}
		}

	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("VideoPlay::LoadVideoURLFromProfile - OwnerOverlay is invalid or m_widget is null."));
	}
	return;
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
	UE_LOG(LogTemp, Log, TEXT("VideoPlay 打开视频流 进入"));
	if (nullptr == m_WrapOpenCv)
	{
		m_WrapOpenCv = new WrapOpenCv();
	}

	if (false == m_WrapOpenCv->m_Stream.open(TCHAR_TO_UTF8(*m_VideoURL)))
	{
		UE_LOG(LogTemp, Error, TEXT("VideoPlay 打开视频失败 url=%s"), *m_VideoURL);
		NotifyFailed();
		return -1;
	}

	m_TotalFrames = m_WrapOpenCv->m_Stream.get(cv::CAP_PROP_FRAME_COUNT);
	if (m_TotalFrames <= 0) {
		UE_LOG(LogTemp, Error, TEXT("VideoPlay 获取视频总帧数失败或为0 url=%s"), *m_VideoURL);
		m_WrapOpenCv->m_Stream.release();
		delete m_WrapOpenCv;
		m_WrapOpenCv = nullptr;
		NotifyFailed();
		return -1;
	}
	UE_LOG(LogTemp, Log, TEXT("VideoPlay 打开视频成功, 总帧数: %d"), m_TotalFrames);

	// 初始化帧索引
	m_CurrentFrameIndex = m_bReverse ? (m_TotalFrames - 1) : 0;
	if (m_TotalFrames > 0) { // 只有在有帧的情况下才设置初始位置
		m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
	}

	m_LastReadTime = FDateTime::Now(); // 初始化上次读取时间

	UE_LOG(LogTemp, Log, TEXT("VideoPlay Run 运行循环 进入"));
	while (false == m_Stopping)
	{
		if (m_bPaused)
		{
			FPlatformProcess::Sleep(0.01f);
			continue;
		}

		if (false == m_WrapOpenCv->m_Stream.isOpened())
		{
			UE_LOG(LogTemp, Error, TEXT("VideoPlay Run 循环中发现视频流未打开"));
			NotifyFailed();
			return -1;
		}

		// --- 非实时模式处理 (优化后) ---
		if (false == m_RealMode)
		{
			// 1. 时间控制: 检查是否到达下一帧的显示时间
			auto DataNow = FDateTime::Now();
			double ElapsedMs = (DataNow - m_LastReadTime).GetTotalMilliseconds();
			double TargetIntervalMs = m_UpdateTime / m_PlayRate; // m_UpdateTime = 1000 / m_Fps

			if (ElapsedMs < TargetIntervalMs)
			{
				// 时间未到，短暂休眠避免忙等
				// 可以根据剩余时间调整休眠时长，但简单固定值通常足够
				FPlatformProcess::Sleep(FMath::Max(0.001f, (float)(TargetIntervalMs - ElapsedMs) / 2000.0f)); // 休眠剩余时间的一半，但不小于1ms
				continue; // 继续检查时间
			}

			// 时间已到或超过，更新上次读取时间 (即使读取失败也要更新，防止卡死)
			// 使用实际经过的时间来计算跳过的帧数，更精确地模拟播放速率
			int32 framesToSkip = FMath::Max(0, FMath::FloorToInt(ElapsedMs / TargetIntervalMs) - 1);
			m_LastReadTime = DataNow; // 或者 m_LastReadTime += FTimespan::FromMilliseconds(TargetIntervalMs * (framesToSkip + 1)); // 理论上更精确，但可能累积误差

			bool bFrameReadSuccess = false;

			// 2. 反向播放处理 (仍然需要 Seek)
			if (m_bReverse)
			{
				// 反向播放优化空间有限，仍然需要seek
				// 可以考虑优化 frameStep 计算，例如基于 TargetIntervalMs 和实际 ElapsedMs
				// 但基本逻辑不变：计算目标帧号 -> seek -> read
				int32 frameStep = FMath::Max(1, FMath::RoundToInt((ElapsedMs / (1000.0 / m_Fps)) * m_PlayRate)); // 基于实际流逝时间和目标帧率计算步进
				m_CurrentFrameIndex -= frameStep;

				if (m_CurrentFrameIndex < 0)
				{
					UE_LOG(LogTemp, Verbose, TEXT("非实时模式: 索引回绕 (反向): Index %d < 0. 重置为最后一帧 %d."), m_CurrentFrameIndex, m_TotalFrames - 1);
					m_CurrentFrameIndex = m_TotalFrames - 1; // 回绕到最后一帧
					// 反向播放完成一轮 (从头到尾)
					if (!m_bFirstPlayCompleted) // 只有首次反向播完触发
					{
						// 注意：反向播放的“完成”概念可能需要重新定义，这里暂时不触发
						// m_bFirstPlayCompleted = true;
						// NotifyFirstPlayCompleted();
					}
					// Seek 到循环点
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
				}
				else
				{
					// 正常反向 Seek
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
				}

				// 尝试读取
				if (m_WrapOpenCv->m_Stream.read(m_WrapOpenCv->m_Frame))
				{
					bFrameReadSuccess = true;
					// 读取成功后，可以获取实际读取到的帧号，更新 m_CurrentFrameIndex 以提高准确性
					// m_CurrentFrameIndex = m_WrapOpenCv->m_Stream.get(cv::CAP_PROP_POS_FRAMES) - 1; // get返回的是下一帧的索引
				}
			}
			// 3. 正向播放处理 (优化核心：顺序读取)
			else // m_bReverse == false
			{
				// 跳过因时间流逝需要跳过的帧 (模拟快进)
				for (int32 i = 0; i < framesToSkip; ++i)
				{
					if (!m_WrapOpenCv->m_Stream.grab()) // grab() 比 read() 更快，因为它只抓取帧数据不解码
					{
						// 如果抓取失败，可能到了视频末尾
						break;
					}
					m_CurrentFrameIndex++; // 也要更新索引计数
				}

				// 读取当前目标帧
				if (m_WrapOpenCv->m_Stream.read(m_WrapOpenCv->m_Frame))
				{
					bFrameReadSuccess = true;
					m_CurrentFrameIndex++; // 更新索引到下一帧的位置
				}

				// 检查是否到达末尾 (读取失败或索引超限)
				if (!bFrameReadSuccess || m_CurrentFrameIndex >= m_TotalFrames)
				{
					UE_LOG(LogTemp, Verbose, TEXT("非实时模式: 到达视频末尾 (读取 %s, Index %d >= %d)."),
						bFrameReadSuccess ? TEXT("成功") : TEXT("失败"), m_CurrentFrameIndex, m_TotalFrames);

					// --- 首次播放完成检测 ---
					if (!m_bFirstPlayCompleted)
					{
						m_bFirstPlayCompleted = true;
						NotifyFirstPlayCompleted();
						UE_LOG(LogTemp, Log, TEXT("NotifyFirstPlayCompleted (非实时模式正向播放完成)"));
					}
					// --- 首次完成检测结束 ---

					// 重置到开头实现循环播放
					m_CurrentFrameIndex = 0;
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
					UE_LOG(LogTemp, Verbose, TEXT("非实时模式: 重置索引为 0 以进行循环播放."));

					// 尝试读取第一帧，否则画面会停留在最后一帧直到下一个时间间隔
					if (m_WrapOpenCv->m_Stream.read(m_WrapOpenCv->m_Frame))
					{
						bFrameReadSuccess = true;
						m_CurrentFrameIndex++; // 更新索引
					}
					else
					{
						// 如果连第一帧都读不了，可能视频有问题
						UE_LOG(LogTemp, Error, TEXT("非实时模式: 循环后读取第一帧失败!"));
						// 这里可以选择停止或继续尝试
					}
				}
			} // 结束正向播放处理

			// 4. 更新纹理 (如果读取成功)
			if (bFrameReadSuccess)
			{
				NotifyFirstFrame(); // 通知首帧（如果尚未通知）
				UpdateTexture();    // 更新纹理
			}
			else
			{
				// 读取失败（非循环点），可以记录日志或进行其他错误处理
				UE_LOG(LogTemp, Warning, TEXT("非实时模式: 在帧 %d 处读取失败 (非循环点)."), m_CurrentFrameIndex);
				// 可以考虑重试或停止
			}

		} // 结束 if (false == m_RealMode)
		// --- 实时模式处理 (保持不变) ---
		else
		{
			FPlatformProcess::Sleep(m_SleepSecond / m_PlayRate);

			if (true == m_WrapOpenCv->m_Stream.read(m_WrapOpenCv->m_Frame))
			{
				NotifyFirstFrame();
				UpdateTexture();
				// 实时模式下也应该更新帧索引，虽然不直接用它来seek
				m_CurrentFrameIndex = m_WrapOpenCv->m_Stream.get(cv::CAP_PROP_POS_FRAMES);
			}
			else
			{
				// 实时模式读取失败，处理循环和完成通知
				if (!m_bReverse) // 正向播放结束
				{
					if (!m_bFirstPlayCompleted)
					{
						m_bFirstPlayCompleted = true;
						NotifyFirstPlayCompleted();
						UE_LOG(LogTemp, Log, TEXT("NotifyFirstPlayCompleted (实时模式读取失败/结束)"));
					}
					// 实时模式循环
					m_CurrentFrameIndex = 0;
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
				}
				else // 反向播放结束 (假设实时反向可行且会失败在开头)
				{
					// 处理反向循环和通知（如果需要）
					m_CurrentFrameIndex = m_TotalFrames - 1;
					m_WrapOpenCv->m_Stream.set(cv::CAP_PROP_POS_FRAMES, m_CurrentFrameIndex);
				}
			}
		} // 结束 else (实时模式)
	} // 结束 while (false == m_Stopping)

	UE_LOG(LogTemp, Log, TEXT("VideoPlay Run 运行循环 结束"));
	// 清理资源 (确保在循环外执行)
	if (m_WrapOpenCv && m_WrapOpenCv->m_Stream.isOpened()) {
		m_WrapOpenCv->m_Stream.release();
	}
	// delete m_WrapOpenCv; // 不在这里delete，StopPlay会处理
	// m_WrapOpenCv = nullptr;

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
	// 1. 使用双缓冲避免每帧分配内存
	static TArray<uint8> PixelBuffer1, PixelBuffer2;
	static TArray<uint8>* CurrentBuffer = &PixelBuffer1;
	static TArray<uint8>* NextBuffer = &PixelBuffer2;

	// 2. 得到最终用于渲染的 Mat —— resizedFrame
	cv::Mat resizedFrame;
	if (m_bCustomResolution)
	{
		cv::resize(m_WrapOpenCv->m_Frame, resizedFrame,
			cv::Size(m_TargetResolution.X, m_TargetResolution.Y));
	}
	else
	{
		resizedFrame = m_WrapOpenCv->m_Frame;
	}

	// 3. 用 resizedFrame 的实际大小后续全部使用
	const int32 NewWidth = resizedFrame.cols;
	const int32 NewHeight = resizedFrame.rows;

	// 4. 判断是否需要重建纹理资源
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
				VideoTexture = UTexture2D::CreateTransient(m_VideoSize.X, m_VideoSize.Y);
				if (VideoTexture)
				{
					VideoTexture->Filter = TF_Bilinear;
					VideoTexture->SRGB = true;
					VideoTexture->CompressionSettings = TC_Default;
					VideoTexture->UpdateResource();
					VideoTexture->AddToRoot();  // 防止被GC
				}
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

		// 预分配一次，避免频繁分配
		if (m_PixelDataBuffer.Num() < NewWidth * NewHeight * 4) {
			m_PixelDataBuffer.SetNumUninitialized(NewWidth * NewHeight * 4);
		}
	}

	// 如果依然没有 Texture，说明可能初始化失败，直接返回
	if (!VideoTexture)
	{
		return;
	}

	// 5. 填充像素数据 (BGR => RGBA)
	FColor* PixelData = reinterpret_cast<FColor*>(m_PixelDataBuffer.GetData());

	// 使用并行处理加速像素转换
	ParallelFor(NewHeight, [&](int32 y) {
		for (int x = 0; x < NewWidth; x++)
		{
			int32 i = x + y * NewWidth;
			int32 srcIdx = i * 3;
			PixelData[i].B = resizedFrame.data[srcIdx];
			PixelData[i].G = resizedFrame.data[srcIdx + 1];
			PixelData[i].R = resizedFrame.data[srcIdx + 2];
			PixelData[i].A = 255;
		}
		});

	// 6. 更新纹理区域
	UpdateTextureRegions(VideoTexture, 0, 1, m_VideoUpdateTextureRegion,
		(uint32)(4 * NewWidth), (uint32)4, (uint8*)PixelData, false);

	// 7. 在Game Thread中设置 UImage 的 Brush
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