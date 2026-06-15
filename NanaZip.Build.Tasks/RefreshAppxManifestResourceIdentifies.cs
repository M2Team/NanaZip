using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;
using Mile.Project.Helpers;
using System.IO;

namespace NanaZip.Build.Tasks
{
    public class RefreshAppxManifestResourceIdentifies : Task
    {
        [Required]
        public string? FilePath { get; set; }

        public override bool Execute()
        {
            string Content = File.ReadAllText(FilePath);

            string PreviousContent = string.Empty;
            {
                string StartString = "  <Resources>\r\n";
                string EndString = "  </Resources>";

                int Start = Content.IndexOf(StartString) + StartString.Length;
                int End = Content.IndexOf(EndString);

                PreviousContent = Content.Substring(Start, End - Start);
            }

            if (string.IsNullOrEmpty(PreviousContent))
            {
                Log.LogError(
                    "Cannot find the Resources node content in the manifest.");
                return false;
            }

            string NewContent = string.Empty;
            {
                NewContent += "    <Resource Language=\"x-generate\"/>\r\n";

                int[] Scales = [100, 125, 150, 200, 400];
                foreach (var Scale in Scales)
                {
                    NewContent += string.Format(
                        "    <Resource uap:Scale=\"{0}\" />\r\n",
                        Scale.ToString());
                }
            }

            FileUtilities.SaveTextToFileAsUtf8Bom(
                FilePath,
                File.ReadAllText(FilePath).Replace(
                    PreviousContent,
                    NewContent));

            return true;
        }
    }
}
