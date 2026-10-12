using System;
using System.IO;
using System.Security.Cryptography;

namespace BlackXboxLauncher
{
    internal static class GameLanguageBanks
    {
        internal const string English = "en-US";
        internal const string PortugueseBrazil = "pt-BR";
        internal const string Spanish = "es";
        internal const string French = "fr";
        internal const string German = "de";
        internal const string Italian = "it";
        internal const string StockEnglishSha256 = "B23EEB4AD86A72894B40E7239D0EA8842E5A6BC05936A936401B608070E8C7BB";

        internal static bool IsSupported(string language)
        {
            return language == English || language == PortugueseBrazil || language == Spanish ||
                language == French || language == German || language == Italian;
        }

        internal static string Normalize(string language)
        {
            return IsSupported(language) ? language : English;
        }

        internal static bool IsMachineDraft(string language)
        {
            return language == Spanish || language == French || language == German || language == Italian;
        }

        internal static string DisplayName(string language)
        {
            switch (language)
            {
                case English: return "English";
                case PortugueseBrazil: return "Português (Brasil)";
                case Spanish: return "Spanish (machine draft)";
                case French: return "French (machine draft)";
                case German: return "German (machine draft)";
                case Italian: return "Italian (machine draft)";
                default: return "English";
            }
        }

        internal static string BankPath(string root, string language)
        {
            if (!IsSupported(language)) throw new ArgumentException("Choose one of the supported game languages.", "language");
            return Path.Combine(Path.GetFullPath(root), "local", "language-banks", language, "MainUS.bin");
        }

        internal static string GamePath(string root)
        {
            return Path.Combine(Path.GetFullPath(root), "game", "language", "strings", "MainUS.bin");
        }

        internal static void Install(string root, string language)
        {
            Install(root, language, StockEnglishSha256);
        }

        internal static void Install(string root, string language, string expectedEnglishSha256)
        {
            string source = BankPath(root, language);
            string target = GamePath(root);
            if (language != English && !File.Exists(source))
                throw new FileNotFoundException("The selected language bank is missing. Restore it under local\\language-banks\\" + language + "\\MainUS.bin.", source);
            if (!File.Exists(target))
                throw new FileNotFoundException("The game's MainUS.bin is missing from game\\language\\strings.", target);

            bool preserved = PreserveEnglishBank(root, target, expectedEnglishSha256);
            if (language == English && preserved) return;
            string temporary = TemporaryPath(target);
            try
            {
                CopyBank(source, temporary);
                if (BankHash(temporary) == BankHash(target)) return;
                File.Replace(temporary, target, null);
            }
            finally
            {
                if (File.Exists(temporary)) File.Delete(temporary);
            }
        }

        private static bool PreserveEnglishBank(string root, string target, string expectedSha256)
        {
            string english = BankPath(root, English);
            if (File.Exists(english)) return false;

            Directory.CreateDirectory(Path.GetDirectoryName(english));
            string temporary = TemporaryPath(english);
            try
            {
                CopyBank(target, temporary);
                string actualSha256 = BankHash(temporary);
                if (!String.Equals(actualSha256, expectedSha256, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("The game's MainUS.bin is not the recognized original English bank. Restore the original English MainUS.bin from your supported retail game files under local\\language-banks\\en-US\\MainUS.bin before changing languages.");

                // Rename within this directory publishes a complete fallback without
                // overwriting an English bank another launcher may have preserved.
                try { File.Move(temporary, english); return true; }
                catch (IOException) { if (!File.Exists(english)) throw; return false; }
            }
            finally
            {
                if (File.Exists(temporary)) File.Delete(temporary);
            }
        }

        private static string TemporaryPath(string target)
        {
            return target + ".launcher-tmp-" + Guid.NewGuid().ToString("N");
        }

        private static string BankHash(string path)
        {
            using (var hash = SHA256.Create())
            using (var stream = File.OpenRead(path))
                return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-", "");
        }

        private static void CopyBank(string source, string temporary)
        {
            using (var input = File.OpenRead(source))
            using (var output = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write))
                input.CopyTo(output);
        }
    }
}
