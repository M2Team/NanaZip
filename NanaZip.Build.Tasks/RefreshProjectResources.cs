using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;
using Mile.Project.Helpers;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace NanaZip.Build.Tasks
{
    public class RefreshProjectResources : Task
    {
        [Required]
        public string? RootPath { get; set; }

        [Required]
        public bool BuildPreviewRelease { get; set; }

        static void ReplaceFileContentViaStringList(
            string FilePath,
            List<string> FromList,
            List<string> ToList)
        {
            if (FromList.Count != ToList.Count)
            {
                throw new ArgumentOutOfRangeException();
            }

            string Content = File.ReadAllText(FilePath, Encoding.UTF8);

            for (int Index = 0; Index < FromList.Count; ++Index)
            {
                Content = Content.Replace(FromList[Index], ToList[Index]);
            }

            if (Path.GetExtension(FilePath) == ".rc")
            {
                File.WriteAllText(FilePath, Content, Encoding.Unicode);
            }
            else
            {
                FileUtilities.SaveTextToFileAsUtf8Bom(FilePath, Content);
            }
        }

        static List<string> ReleaseStringList = new List<string>
        {
            "DisplayName=\"NanaZip\"",
            "Name=\"40174MouriNaruto.NanaZip\"",
            "<DisplayName>NanaZip</DisplayName>",
            "CAE3F1D4-7765-4D98-A060-52CD14D56EAB",
            "return ::SHStrDupW(L\"NanaZip\", ppszName);",
            "<Content Include=\"..\\Assets\\PackageAssets\\**\\*\">",
            "Assets/NanaZip.ico",
            "Assets/NanaZipSfx.ico",
        };

        static List<string> PreviewStringList = new List<string>
        {
            "DisplayName=\"NanaZip Preview\"",
            "Name=\"40174MouriNaruto.NanaZipPreview\"",
            "<DisplayName>NanaZip Preview</DisplayName>",
            "469D94E9-6AF4-4395-B396-99B1308F8CE5",
            "return ::SHStrDupW(L\"NanaZip Preview\", ppszName);",
            "<Content Include=\"..\\Assets\\PreviewPackageAssets\\**\\*\">",
            "Assets/NanaZipPreview.ico",
            "Assets/NanaZipPreviewSfx.ico",
        };

        static List<string> FileList = new List<string>
        {
            @"{0}\NanaZip.Core\SevenZip\CPP\7zip\Bundles\SFXCon\resource.rc",
            @"{0}\NanaZip.Core\SevenZip\CPP\7zip\Bundles\SFXSetup\resource.rc",
            @"{0}\NanaZip.Core\SevenZip\CPP\7zip\Bundles\SFXWin\resource.rc",
            @"{0}\NanaZip.Universal\SevenZip\CPP\7zip\UI\GUI\resource.rc",
            @"{0}\NanaZip.UI.Modern\SevenZip\CPP\7zip\UI\FileManager\resource.rc",
            @"{0}\NanaZip.UI.Modern\NanaZip.ShellExtension.cpp",
            @"{0}\NanaZipPackage\Package.appxmanifest",
            @"{0}\NanaZipPackage\NanaZipPackage.wapproj",
        };

        public override bool Execute()
        {
            foreach (var FilePath in FileList)
            {
                ReplaceFileContentViaStringList(
                    string.Format(FilePath, RootPath),
                    BuildPreviewRelease ? ReleaseStringList : PreviewStringList,
                    BuildPreviewRelease ? PreviewStringList : ReleaseStringList);
            }

            return true;
        }
    }
}
