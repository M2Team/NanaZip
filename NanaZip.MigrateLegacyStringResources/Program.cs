using Microsoft.Build.Construction;
using System.Collections.Immutable;
using System.Xml;

namespace NanaZip.MigrateLegacyStringResources;

public record ResourceMapping(
    int ResourceKey,
    string File,
    string NewResourcePath,
    bool RemoveAmpersand = false
);

internal class Program
{
    const string StringsDir = "Strings";
    const string LegacyDir = $"NanaZipPackage\\{StringsDir}";
    const string NewDir = $"NanaZip.Modern\\{StringsDir}";
    const string OldFileName = @"Legacy.resw";

    static readonly ImmutableArray<ResourceMapping> map =
        [
            new(408, "InformationPage", "CloseButton.Content", true)
        ];

    static readonly ImmutableArray<IGrouping<string, ResourceMapping>> Mappings
        = map.GroupBy(x => x.File).ToImmutableArray();

    static int Main(string[] args)
    {
        string gitRoot = Mile.Project.Helpers.GitRepository.GetRootPath();

        if (string.IsNullOrEmpty(gitRoot))
        {
            Console.WriteLine("This tool must be placed in a Git repository.");
            return -1;
        }

        string? vslocation = VSWhereHelper.GetVSPath();
        if (string.IsNullOrEmpty(vslocation))
        {
            Console.WriteLine("Visual Studio must be installed.");
            return -2;
        }

        string reswTemplatePath =
            $"{vslocation}\\Common7\\IDE\\ItemTemplates\\WapProj\\" +
            $"1033\\Resw\\Resources.resw";

        List<(string Lang, string File)> files = new();

        foreach (
            DirectoryInfo oldSubDirs
            in
            new DirectoryInfo($"{gitRoot}\\{LegacyDir}")
            .GetDirectories())
        {
            string language = oldSubDirs.Name;
            string oldFile = $"{oldSubDirs.FullName}\\{OldFileName}";
            XmlDocument oldDoc = new XmlDocument();
            oldDoc.Load(oldFile);

            foreach (var file in Mappings)
            {
                string fileName = file.Key;
                XmlDocument newDoc = new();
                try
                {
                    newDoc.Load($"{gitRoot}\\{NewDir}\\{language}\\{fileName}.resw");
                }
                catch
                {
                    newDoc.Load(reswTemplatePath);
                }

                foreach (var mapping in file)
                {
                    XmlNode? oldNode = oldDoc.SelectSingleNode(
                        $"/root/data[@name='Resource{mapping.ResourceKey}']/value"
                    );

                    if (oldNode is null)
                        continue;

                    string newText = oldNode.InnerText ?? string.Empty;
                    if (mapping.RemoveAmpersand)
                    {
                        newText = newText.Replace("&", string.Empty);
                    }

                    XmlNode? newNode = newDoc.SelectSingleNode(
                        $"/root/data[@name='{mapping.NewResourcePath}']/value"
                    );

                    if (newNode is not null)
                    {
                        newNode.InnerText = newText;
                        continue;
                    }

                    XmlElement data = newDoc.CreateElement("data");
                    data.SetAttribute("name", mapping.NewResourcePath);
                    data.SetAttribute("xml:space", "preserve");
                    XmlElement dataValue = newDoc.CreateElement("value");
                    dataValue.InnerText = newText;
                    data.AppendChild(dataValue);
                    newDoc.DocumentElement!.AppendChild(data);
                }

                Directory.CreateDirectory($"{gitRoot}\\{NewDir}\\{language}");
                files.Add((language, $"{StringsDir}\\{language}\\{fileName}.resw"));

                using FileStream stream = new(
                    $"{gitRoot}\\{NewDir}\\{language}\\{fileName}.resw",
                    FileMode.OpenOrCreate);
                stream.SetLength(0);
                using XmlWriter writer = XmlWriter.Create(
                    stream,
                    new XmlWriterSettings() { Indent = true });
                newDoc.WriteTo(writer);
            }
        }

        var project = ProjectRootElement.Open(
            $"{gitRoot}\\NanaZip.Modern\\NanaZip.Modern.vcxproj"
        );

        var excludedFiles = files
            .Select(x => x.File)
            .Except(
            project.Items.Where(x => x.ItemType == "PRIResource")
            .Select(x => x.Include));
        foreach (var file in excludedFiles)
        {
            var item = project.AddItem("PRIResource", file);
        }
        project.Save();

        var filterProject = ProjectRootElement.Open(
            $"{gitRoot}\\NanaZip.Modern\\NanaZip.Modern.vcxproj.filters"
        );
        foreach (var file in files)
        {
            var hasItem = filterProject.Items
                .Any(x =>
                x.ItemType == "PRIResource" &&
                x.Include == file.File);
            if (!hasItem)
            {
                var item = filterProject.AddItem("PRIResource", file.File);
                item.AddMetadata("Filter", $"{StringsDir}\\{file.Lang}");
            }
        }

        foreach (var lang in files.Select(x => x.Lang).Distinct())
        {
            var hasItem = filterProject.Items
                .Any(
                x =>
                x.ItemType == "Filter" &&
                x.Include == $"{StringsDir}\\{lang}");
            if (!hasItem)
            {
                var item = filterProject.AddItem(
                    "Filter",
                    $"{StringsDir}\\{lang}");
                item.AddMetadata("UniqueIdentifier",
                    Guid.NewGuid().ToString("B"));
            }
        }
        filterProject.Save();

        Console.WriteLine("Finished migrating resources.");
        return 0;
    }
}
