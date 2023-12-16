#include "AITestsCommon.h"
#include "FileHelpers.h"
#include "Algo/Accumulate.h"
#include "Editor/EditorEngine.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Performance/EnginePerformanceTargets.h"
#include "Performance/LyraPerformanceStatSubsystem.h"
#include "Performance/LyraPerformanceStatTypes.h"
#include "Tests/AutomationCommon.h"

extern ENGINE_API float GAverageFPS;
extern ENGINE_API uint32 GRenderThreadTime;
extern ENGINE_API uint32 GRHIThreadTime;
extern ENGINE_API uint32 GGPUFrameTime;
extern ENGINE_API uint32 GGameThreadTime; 


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FLoadLevelAndCheckFrameRateTest, "Project.LevelFramerateTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FLoadLevelAndCheckFrameRateTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
    TArray<FString> FileList;
    FEditorFileUtils::FindAllPackageFiles(FileList);

    // Iterate over all files, adding the ones with the map extension..
    for( int32 FileIndex=0; FileIndex< FileList.Num(); FileIndex++ )
    {
        const FString& Filename = FileList[FileIndex];

        // Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
        if ( FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension()) 
        {
            if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
            {
                if (!Filename.Contains(TEXT("/Engine/")))
                {
                    OutBeautifiedNames.Add(FPaths::GetBaseFilename(Filename));
                    OutTestCommands.Add(Filename);
                }
            }
        }
    }
}

/** Adds an easy way to average some value over discrete updates.  */
template <typename T>
struct TWaitForFrameRateRollingAverage
{
    static_assert(TIsArithmetic<T>::Value, "Unsupported type for computing a rolling average...");

    TArray<T> Buffer;
    int32 BufferOffset = 0;

    void SetNum(int32 NewSize)
    {
        // at least 1
        Buffer.SetNum(FMath::Max(NewSize, 1));
        BufferOffset = BufferOffset % Buffer.Num();
    }

    void Reset()
    {
        BufferOffset = 0;
    }

    void Add(const T Value)
    {
        Buffer[BufferOffset] = Value;
        BufferOffset = (BufferOffset + 1) % Buffer.Num();
    }

    const T Average() const
    {
        const int32 Num = Buffer.Num();
        return Num > 0 ? Algo::Accumulate(Buffer, 0) / Num : 0;
    }
};


class FMeasureAverageFrameRateCommand : public IAutomationLatentCommand
{
public:
    FMeasureAverageFrameRateCommand(float InDuration, bool& bFailed)
        : Duration(InDuration), bTestFailed(bFailed), StartTimeOfWait(0.0), LastTickTime(0.0), BufferIndex(0)
    {
        RollingTickRateBuffer.SetNum(KSampleCount);

        UWorld* World = nullptr;

#if WITH_EDITOR
        if (GEditor && GEditor->PlayWorld)
        {
            World = GEditor->PlayWorld;
        }
#else
        UWorld* World = GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::LogAndReturnNull);
#endif

        if (World && World->GetGameInstance())
        {
            PerformanceStatSubsystem = World->GetGameInstance()->GetSubsystem<ULyraPerformanceStatSubsystem>();
        }
    }
    
    virtual bool Update() override
    {

        if (!PerformanceStatSubsystem)
        {
            UE_LOG(LogTemp, Warning, TEXT("PerformanceStatSubsystem is not valid."));
            return true; // Finish execution
        }

#if !UE_BUILD_SHIPPING
        // Multi-GPU support : ChartCreation doesn't support MGPU yet
        FPlatformMisc::CustomNamedStat("NumDrawCallsRHI", (float)GNumDrawCallsRHI[0], "Rendering", "Count");
        FPlatformMisc::CustomNamedStat("NumPrimitivesDrawnRHI", (float)GNumPrimitivesDrawnRHI[0], "Rendering", "Count");

        FPlatformMisc::CustomNamedStat("MemoryUsed", (float)FPlatformMemory::GetMemoryUsedFast(), "Memory", "Bytes");
#endif // !UE_BUILD_SHIPPING

        MaxDrawCalls = FMath::Max<uint32>(MaxDrawCalls, GNumDrawCallsRHI[0]);
        MinDrawCalls = FMath::Min<uint32>(MinDrawCalls, GNumDrawCallsRHI[0]);
        TotalDrawCalls += GNumDrawCallsRHI[0];
        const uint32 LocalRenderThreadTime = GRenderThreadTime;
        const uint32 LocalRHIThreadTime = GRHIThreadTime;
        const uint32 LocalGPUFrameTime = GGPUFrameTime;
        double ThisFrameIdleTime = FApp::GetIdleTime();
        double ThisFrameIdleTimeOvershoot = FApp::GetIdleTimeOvershoot();

        // determine which pipeline time is the greatest (between game thread, render thread, and GPU)
        constexpr float EpsilonCycles = 0.250f;
        const uint32 MaxThreadTimeValue = FMath::Max(TArray<uint32> {LocalRenderThreadTime, GGameThreadTime, LocalGPUFrameTime, LocalRHIThreadTime});
        const float FrameTime = FPlatformTime::ToSeconds(MaxThreadTimeValue);

        const float GameThreadTimeSeconds = FPlatformTime::ToSeconds(GGameThreadTime);
        const float RenderThreadTimeSeconds = FPlatformTime::ToSeconds(LocalRenderThreadTime);
        const float RHIThreadTimeSeconds = FPlatformTime::ToSeconds(LocalRHIThreadTime);
        const float GPUTimeSeconds = FPlatformTime::ToSeconds(LocalGPUFrameTime);

        extern COREUOBJECT_API double GFlushAsyncLoadingTime;
        extern COREUOBJECT_API uint32 GFlushAsyncLoadingCount;
        extern COREUOBJECT_API uint32 GSyncLoadCount;

        double FlushAsyncLoadingTime = GFlushAsyncLoadingTime;
        uint32 FlushAsyncLoadingCount = GFlushAsyncLoadingCount;
        uint32 SyncLoadCount = GSyncLoadCount;

        constexpr float GMaximumFrameTimeToConsiderForHitchesAndBinning = 10.0f;


        
        double CurrentTime = FPlatformTime::Seconds();

        if (StartTimeOfWait == 0.0)
        {
            StartTimeOfWait = CurrentTime;
        }

        //double TickDelta = CurrentTime - LastTickTime;
        double TickDelta = CurrentTime - LastTickTime;
        LastTickTime = CurrentTime;

        const float EngineTargetMS = FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();
        constexpr float MSToSeconds = 1.0f / 1000.0f;
        
        float bBinThisFrame = (TickDelta < GMaximumFrameTimeToConsiderForHitchesAndBinning) || (GMaximumFrameTimeToConsiderForHitchesAndBinning <= 0.0f);

        bool bGameThreadBound = false;
        bool bRenderThreadBound = false;
        bool bRHIThreadBound = false;
        bool bGPUBound = false;

        // if frame time is greater than our target then we are bounded by something
        const float TargetThreadTimeSeconds = EngineTargetMS * MSToSeconds;
        if (TickDelta > TargetThreadTimeSeconds)
        {
            if (GameThreadTimeSeconds >= TargetThreadTimeSeconds)
            {
                bGameThreadBound = true;
            }

            if (RenderThreadTimeSeconds >= TargetThreadTimeSeconds)
            {
                bRenderThreadBound = true;
            }

            if (RHIThreadTimeSeconds >= TargetThreadTimeSeconds)
            {
                bRHIThreadBound = true;
            }

            // Consider this frame GPU bound if we have an actual measurement which is over the limit,
            if (GPUTimeSeconds >= TargetThreadTimeSeconds)
            {
                bGPUBound = true;
            }
        }
        const double FrameRate = 1.f / TickDelta;
        
        // Add frame time sample
        AddTickRateSample(FrameTime);

        double ElapsedTime = CurrentTime - StartTimeOfWait;

        static int LastSecond = 0;

         if (ElapsedTime > LastSecond)
         {
        // Concatenate all variables into a single log message
        FString LogMessage = FString::Printf(
            TEXT("Frame rate: %f, GameThreadTime: %f, RenderThreadTime: %f, RHIThreadTime: %f, GPUTime: %f, FlushAsyncLoadingTime: %f, FlushAsyncLoadingCount: %u, SyncLoadCount: %u, MaxDrawCalls: %u, MinDrawCalls: %u, TotalDrawCalls: %llu, Bound: GameThread=%s, RenderThread=%s, RHIThread=%s, GPU=%s"),
            FrameRate,
            GameThreadTimeSeconds,
            RenderThreadTimeSeconds,
            RHIThreadTimeSeconds,
            GPUTimeSeconds,
            FlushAsyncLoadingTime,
            FlushAsyncLoadingCount,
            SyncLoadCount,
            MaxDrawCalls,
            MinDrawCalls,
            TotalDrawCalls,
            bGameThreadBound ? TEXT("True") : TEXT("False"),
            bRenderThreadBound ? TEXT("True") : TEXT("False"),
            bRHIThreadBound ? TEXT("True") : TEXT("False"),
            bGPUBound ? TEXT("True") : TEXT("False")
        );

        // Log the concatenated message
        UE_LOG(LogTemp, Log, TEXT("%s"), *LogMessage);
            LastSecond = FMath::CeilToInt(ElapsedTime);
         }

        // Accumulate times for average calculations
        TotalGameThreadTime += GameThreadTimeSeconds * 1000;
        TotalRenderThreadTime += RenderThreadTimeSeconds  * 1000;
        TotalRHIThreadTime += RHIThreadTimeSeconds * 1000;
        TotalGPUTime += GPUTimeSeconds * 1000;
        TotalFlushAsyncLoadingTime += FlushAsyncLoadingTime;

        // Accumulate counts
        TotalFlushAsyncLoadingCount += FlushAsyncLoadingCount;
        TotalSyncLoadCount += SyncLoadCount;
        FrameCount++;

        if (TickDelta < (1 / kTickRate))
        {
            return false; // End execution
        }
        
        if (ElapsedTime >= Duration)
        {
// Calculate averages
            float AvgGameThreadTime = CalculateAverageTime(TotalGameThreadTime);
            float AvgRenderThreadTime = CalculateAverageTime(TotalRenderThreadTime);
            float AvgRHIThreadTime = CalculateAverageTime(TotalRHIThreadTime);
            float AvgGPUTime = CalculateAverageTime(TotalGPUTime);
            float AvgFlushAsyncLoadingTime = CalculateAverageTime(TotalFlushAsyncLoadingTime);
            float AvgFlushAsyncLoadingCount = static_cast<float>(TotalFlushAsyncLoadingCount) / FrameCount;
            float AvgSyncLoadCount = static_cast<float>(TotalSyncLoadCount) / FrameCount;

            // Calculate and log average frame rate
            double AverageFrameRate = 1.0 / CurrentAverageTickRate();
            UE_LOG(LogTemp, Log, TEXT("Average Frame Rate over %f seconds: %f FPS"), Duration, AverageFrameRate);

            // Log other averages
            UE_LOG(LogTemp, Log, TEXT("Average Game Thread Time: %f ms, Average Render Thread Time: %f ms, Average RHI Thread Time: %f ms, Average GPU Time: %f ms, Average Flush Async Loading Time: %f ms, Average Flush Async Loading Count: %f, Average Sync Load Count: %f"), AvgGameThreadTime, AvgRenderThreadTime, AvgRHIThreadTime, AvgGPUTime, AvgFlushAsyncLoadingTime, AvgFlushAsyncLoadingCount, AvgSyncLoadCount);
            UE_LOG(LogTemp, Log, TEXT("Max Draw Calls: %u, Min Draw Calls: %u, Total Draw Calls: %llu"), MaxDrawCalls, MinDrawCalls, TotalDrawCalls);


            if (AverageFrameRate < 60.0)
            {
                // Fail the test if the average frame rate is below 60 FPS
                UE_LOG(LogTemp, Error, TEXT("Average Frame Rate is below 60 FPS! Test failed."));
                bTestFailed = true; // Set test failed flag
                return true; // Finish execution
            }
        
            return true; // Finish execution 
        }

        return false; // Continue execution
    }

private:
    bool HasTestFailed() const
    {
        return bTestFailed;
    }
    
    void AddTickRateSample(const double Value)
    {
        RollingTickRateBuffer[BufferIndex] = Value;
        BufferIndex = (BufferIndex + 1) % KSampleCount;
    }

    double CurrentAverageTickRate() const
    {
        return RollingTickRateBuffer.Num() > 0 ? Algo::Accumulate(RollingTickRateBuffer, 0.0) / RollingTickRateBuffer.Num() : 0.0;
    }

    float CalculateAverageTime(uint64 TotalTime) const
    {
        return FrameCount > 0 ? static_cast<float>(TotalTime) / FrameCount : 0.0f;
    }


    float Duration;
    bool bTestFailed;
    double StartTimeOfWait;
    double LastTickTime;
    TArray<double> RollingTickRateBuffer;
    int32 BufferIndex;
    const double kTickRate = 60.0;
    int KSampleCount = kTickRate * Duration;
    ULyraPerformanceStatSubsystem* PerformanceStatSubsystem;
    uint32 MaxDrawCalls = 0;
    uint32 MinDrawCalls = TNumericLimits<uint32>::Max();
    uint64 TotalDrawCalls = 0;

    // Accumulators for average calculations
    uint64 TotalGameThreadTime = 0;
    uint64 TotalRenderThreadTime = 0;
    uint64 TotalRHIThreadTime = 0;
    uint64 TotalGPUTime = 0;
    uint64 TotalFlushAsyncLoadingTime = 0;

    uint32 FrameCount = 0;
    uint32 TotalFlushAsyncLoadingCount = 0;
    uint32 TotalSyncLoadCount = 0;
};

bool FLoadLevelAndCheckFrameRateTest::RunTest(const FString& Parameters)
{
    bool bFailed = false;
    FString MapName = Parameters;
    UE_LOG(LogTemp, Log, TEXT("Loading map: %s"), *MapName);
    AutomationOpenMap(MapName);

    ADD_LATENT_AUTOMATION_COMMAND(FMeasureAverageFrameRateCommand(60.0f, bFailed));

    return !bFailed;
}

#endif
