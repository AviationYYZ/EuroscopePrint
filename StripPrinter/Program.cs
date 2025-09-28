
using System;
using System.Drawing;
using System.Drawing.Printing;
using System.IO;

namespace StripPrinter
{
    internal class Program
    {
        static string Payload = "";

        [STAThread]
        static void Main(string[] args)
        {
            // Args:
            //   --file <path>  : read payload from UTF-8 text file
            //   --text "<txt>" : payload in argument (not recommended for long strips)
            //   --test         : print a test strip
            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--file" && i + 1 < args.Length)
                {
                    Payload = File.ReadAllText(args[++i]);
                }
                else if (args[i] == "--text" && i + 1 < args.Length)
                {
                    Payload = args[++i];
                }
                else if (args[i] == "--test")
                {
                    Payload = StripTemplates.TestStrip();
                }
            }

            if (string.IsNullOrWhiteSpace(Payload))
            {
                Console.Error.WriteLine("No payload provided. Use --test, --file <path>, or --text \"...\"");
                Environment.Exit(2);
            }

            var pd = new PrintDocument();
            pd.PrintPage += OnPrintPage;

            // Print to default printer
            try
            {
                pd.Print();
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine("Print failed: " + ex.Message);
                Environment.Exit(1);
            }
        }

        static void OnPrintPage(object? sender, PrintPageEventArgs e)
        {
            // Basic monospace text rendering
            using var font = new Font(FontFamily.GenericMonospace, 9.0f, FontStyle.Regular, GraphicsUnit.Point);
            var marginBounds = e.MarginBounds;
            var layout = new RectangleF(marginBounds.Left, marginBounds.Top, marginBounds.Width, marginBounds.Height);

            e.Graphics.DrawString(Payload, font, Brushes.Black, layout);
            e.HasMorePages = false;
        }
    }
}
