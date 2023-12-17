##include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/EditorAssetSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetUsageTest, "Project.AssetUsageTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAssetUsageTest::RunTest(const FString& Parameters)
{
    FString CsvContent = "Asset Name,Asset Path,Asset Type,Referenced By,Source Tag,Asset Size,Asset Classification\n"; 

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAllAssets(AssetDataList, true);

    UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();

    TArray<FString> UnusedAssetList;
    bool bHasUnusedAssets = false;

    for (const FAssetData& Asset : AssetDataList)
    {
        FString AssetPath = Asset.GetObjectPathString();
        if (AssetPath.StartsWith(TEXT("/Game/")))
        {
            FString AssetName = Asset.AssetName.ToString();
            FString AssetType = Asset.AssetClassPath.ToString();

            UObject* AssetObject = Asset.GetAsset();
            FString SourceMetadata = EditorAssetSubsystem->GetMetadataTag(AssetObject, FName("Source"));

            TArray<FName> Referencers;
            AssetRegistryModule.Get().GetReferencers(Asset.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);

            int64 AssetSize = 0;
            if (FAssetPackageData AssetPackageData; AssetRegistryModule.Get().TryGetAssetPackageData(Asset.PackageName, AssetPackageData) == UE::AssetRegistry::EExists::Exists)
            {
                AssetSize = AssetPackageData.DiskSize;
            }

            FString AssetClassification = TEXT("Used");
            if (Referencers.Num() == 0)
            {
                AssetClassification = TEXT("Unused");
                UnusedAssetList.Add(AssetPath);
                bHasUnusedAssets = true;
            }

            FString EscapedAssetName = AssetName.Replace(TEXT(","), TEXT(" "));
            FString EscapedAssetPath = AssetPath.Replace(TEXT(","), TEXT(" "));
            FString EscapedAssetType = AssetType.Replace(TEXT(","), TEXT(" "));
            FString EscapedReferencersList = FString::JoinBy(Referencers, TEXT("; "), [](const FName& Name) { return Name.ToString(); });
            FString EscapedSourceMetadata = SourceMetadata.Replace(TEXT(","), TEXT(" "));

            CsvContent += FString::Printf(TEXT("%s,%s,%s,%s,%s,%lld,%s\n"), *EscapedAssetName, *EscapedAssetPath, *EscapedAssetType, *EscapedReferencersList, *EscapedSourceMetadata, AssetSize, *AssetClassification);
        }
    }

    FString CsvFilePath = FPaths::ProjectLogDir() / TEXT("AssetUsageReport.csv");
    if (FFileHelper::SaveStringToFile(CsvContent, *CsvFilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("CSV file saved successfully at: %s"), *CsvFilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save CSV file."));
        AddError(FString::Printf(TEXT("Failed to save CSV file to: %s"), *CsvFilePath));
    }

    if (bHasUnusedAssets)
    {
        for (const FString& UnusedAsset : UnusedAssetList)
        {
            UE_LOG(LogTemp, Warning, TEXT("Unused Asset: %s"), *UnusedAsset);
        }
        AddError("There are unused assets in the project.");
    }

    return !bHasUnusedAssets;
}

#endif // WITH_DEV_AUTOMATION_TESTS
