
using System.Text;

namespace StripPrinter
{
    internal static class StripTemplates
    {
        public static string TestStrip()
        {
            var sb = new StringBuilder();
            sb.AppendLine("=============== TEST FLIGHT STRIP ===============");
            sb.AppendLine("CS: C-GPT5   DEP: CYYZ   ARR: CYHZ");
            sb.AppendLine("ROUTE: DCT YCF J576 YRI J563 ABBOT DCT");
            sb.AppendLine("FL: 350   EOBT: 1530Z   WTC: M");
            sb.AppendLine("EQUIP: SDFGHIRWY / PBN A1B2C3");
            sb.AppendLine("=================================================");
            return sb.ToString();
        }
    }
}
