using System.Diagnostics;

namespace NanaZip.MigrateLegacyStringResources;

public static class VSWhereHelper
{
    public static string? GetVSPath()
    {
        Process process = new()
        {
            StartInfo = new ProcessStartInfo()
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                FileName = "vswhere.exe",
                Arguments = "-prerelease -latest -property resolvedInstallationPath"
            }
        };

        if (!process.Start())
        {
            return null;
        }

        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            return null;
        }

        return process.StandardOutput.ReadLine();
    }
}
