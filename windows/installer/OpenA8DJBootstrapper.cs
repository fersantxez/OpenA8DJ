using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;

internal static class OpenA8DJBootstrapper
{
    private static string QuoteArgument(string value)
    {
        if (value.Length == 0)
        {
            return "\"\"";
        }

        if (value.IndexOfAny(new[] { ' ', '\t', '\"' }) < 0)
        {
            return value;
        }

        return "\"" + value.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
    }

    private static Stream OpenPayload()
    {
        Assembly assembly = Assembly.GetExecutingAssembly();
        string resourceName = assembly.GetManifestResourceNames()
            .FirstOrDefault(name => name.EndsWith("-installer.zip", StringComparison.OrdinalIgnoreCase));
        if (resourceName == null)
        {
            throw new InvalidOperationException("Embedded OpenA8DJ installer payload was not found.");
        }

        Stream payload = assembly.GetManifestResourceStream(resourceName);
        if (payload == null)
        {
            throw new InvalidOperationException("Embedded OpenA8DJ installer payload could not be opened.");
        }

        return payload;
    }

    private static int RunInstaller(string[] args)
    {
        string extractRoot = Path.Combine(Path.GetTempPath(), "OpenA8DJUsb-" + Guid.NewGuid().ToString("N"));
        string zipPath = Path.Combine(Path.GetTempPath(), "OpenA8DJUsb-" + Guid.NewGuid().ToString("N") + ".zip");
        Directory.CreateDirectory(extractRoot);

        try
        {
            using (Stream payload = OpenPayload())
            using (FileStream output = File.Create(zipPath))
            {
                payload.CopyTo(output);
            }

            ZipFile.ExtractToDirectory(zipPath, extractRoot);
            string installCmd = Path.Combine(extractRoot, "install.cmd");
            if (!File.Exists(installCmd))
            {
                throw new InvalidOperationException("The embedded installer payload does not contain install.cmd.");
            }

            if (args.Any(argument => string.Equals(argument, "--verify-only", StringComparison.OrdinalIgnoreCase)))
            {
                string[] requiredFiles =
                {
                    "driver\\OpenA8DJUsb.inf",
                    "driver\\OpenA8DJUsb.sys",
                    "driver\\OpenA8DJUsb.cat",
                    "scripts\\install-driver.ps1",
                    "README-FIRST.txt"
                };
                foreach (string requiredFile in requiredFiles)
                {
                    if (!File.Exists(Path.Combine(extractRoot, requiredFile)))
                    {
                        throw new InvalidOperationException("Embedded installer payload is missing " + requiredFile + ".");
                    }
                }
                return 0;
            }

            string arguments = string.Join(" ", args.Select(QuoteArgument));
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = installCmd,
                Arguments = arguments,
                WorkingDirectory = extractRoot,
                UseShellExecute = true,
                Verb = "runas"
            };

            using (Process process = Process.Start(startInfo))
            {
                process.WaitForExit();
                return process.ExitCode;
            }
        }
        finally
        {
            try { File.Delete(zipPath); } catch { }
            try { Directory.Delete(extractRoot, true); } catch { }
        }
    }

    public static int Main(string[] args)
    {
        string diagnosticPath = Path.Combine(Path.GetTempPath(), "OpenA8DJBootstrapper-last-error.txt");
        try
        {
            File.WriteAllText(diagnosticPath, "started\n" + string.Join("\n", args));
            return RunInstaller(args);
        }
        catch (Exception error)
        {
            try
            {
                File.WriteAllText(
                    diagnosticPath,
                    error.ToString());
            }
            catch { }
            MessageBox.Show(
                error.Message,
                "OpenA8DJ Windows experimental installer",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return 1;
        }
    }
}
