#include "CoreMinimal.h"
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
    FString CsvContent = "Asset Name,Asset Path,Asset Type,Is Used,Referenced By,Source Tag\n"; 

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAllAssets(AssetDataList);

    UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();


    TArray<FString> UnusedAssetList;
    bool bHasUnusedAssets = false;

    for (const FAssetData& Asset : AssetDataList)
    {
        FString AssetPath = Asset.GetSoftObjectPath().ToString();

        // Check if the asset is part of the project (not an Engine asset)
        if (AssetPath.StartsWith(TEXT("/Game/")))
        {
            FString AssetName = Asset.AssetName.ToString();
            FString AssetType = Asset.AssetClassPath.ToString();

            UObject* AssetObject = Asset.GetAsset();
            FString SourceMetadata = EditorAssetSubsystem->GetMetadataTag(AssetObject, FName("Source"));

            TArray<FName> Referencers;
            AssetRegistryModule.Get().GetReferencers(Asset.PackageName, Referencers);

            FString ReferencersList = TEXT("");
            for (const FName& Referencer : Referencers)
            {
                ReferencersList += Referencer.ToString() + TEXT("; ");
            }

            bool bIsUsed = Referencers.Num() > 0;
            if (!bIsUsed)
            {
                UnusedAssetList.Add(AssetPath);
                bHasUnusedAssets = true;
            }

            // Escape commas in data and append to CSV string
            FString EscapedAssetName = AssetName.Contains(TEXT(",")) ? TEXT("\"") + AssetName + TEXT("\"") : AssetName;
            FString EscapedAssetPath = AssetPath.Contains(TEXT(",")) ? TEXT("\"") + AssetPath + TEXT("\"") : AssetPath;
            FString EscapedAssetType = AssetType.Contains(TEXT(",")) ? TEXT("\"") + AssetType + TEXT("\"") : AssetType;
            FString EscapedReferencersList = ReferencersList.Contains(TEXT(",")) ? TEXT("\"") + ReferencersList + TEXT("\"") : ReferencersList;
            FString EscapedSourceMetadata = SourceMetadata.Contains(TEXT(",")) ? TEXT("\"") + SourceMetadata + TEXT("\"") : SourceMetadata;
   
            CsvContent += FString::Printf(TEXT("%s,%s,%s,%s,%s,%s\n"), *EscapedAssetName, *EscapedAssetPath, *EscapedAssetType, bIsUsed ? TEXT("Yes") : TEXT("No"), *EscapedReferencersList, *EscapedSourceMetadata);
   }
    }

    // Generate CSV file path
    FString CsvFilePath = FPaths::ProjectLogDir() / TEXT("AssetUsageReport.csv");
    UE_LOG(LogTemp, Log, TEXT("Saving CSV to: %s"), *CsvFilePath);

    // Save to CSV
    if (FFileHelper::SaveStringToFile(CsvContent, *CsvFilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("CSV file saved successfully."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save CSV file."));
        AddError(FString::Printf(TEXT("Failed to save CSV file to: %s"), *CsvFilePath));
    }

    // Report unused assets
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
