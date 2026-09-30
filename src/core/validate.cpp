/* Headless circuit validator: checks a .atanua file without a window,
 * GL context, audio, or config. Reports machine-readable diagnostics:
 *   OK: <chips> chips, <wires> wires     (exit 0)
 *   ERROR ...                             (exit 1, one line per problem)
 * Usage errors (no path, unreadable file) exit 2.
 * Mirrors the loader's own rules (do_loadxml in fileio.cpp): pad indices
 * are positions in the chip's pin list, chip indices are positions in the
 * document, names match case-insensitively, Box subfiles (*.atanua) carry
 * dynamic pins. Anything the loader would silently drop is an error here.
 */
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <tinyxml2.h>
#include "chipdb.h"

// UTF-8-safe open lives in nativefunctions.cpp (the unit test stubs it);
// declared locally like the other cross-file helpers in this codebase.
FILE *atanua_fopen_rb(const char *aPath);

int validate_circuit_to(const char *aPath, FILE *out)
{
    if (!aPath || !aPath[0])
    {
        fprintf(out, "usage: atanua --validate <file.atanua>\n");
        return 2;
    }
    FILE *fh = atanua_fopen_rb(aPath);
    if (!fh)
    {
        fprintf(out, "ERROR: cannot open file\n");
        return 2;
    }
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(fh) != tinyxml2::XML_SUCCESS)
    {
        fclose(fh);
        fprintf(out, "ERROR: XML parse failed: %s\n", doc.ErrorStr());
        return 1;
    }
    fclose(fh);

    tinyxml2::XMLElement *root = doc.FirstChildElement("Atanua");
    if (!root)
    {
        fprintf(out, "ERROR: missing <Atanua> root element\n");
        return 1;
    }

    std::vector<std::string> names;
    std::vector<int> pincounts;
    std::vector<int> dynamic_;
    int errors = 0;
    int wires = 0;

    for (tinyxml2::XMLElement *part = root->FirstChildElement(); part != 0;
         part = part->NextSiblingElement())
    {
        if (chipNameEq(part->Value(), "Chip"))
        {
            const char *temp = part->Attribute("Name");
            if (!temp)
            {
                fprintf(out,"ERROR line %d: Chip without Name\n", part->GetLineNum());
                errors++;
                names.push_back("");
                pincounts.push_back(0);
                dynamic_.push_back(0);
                continue;
            }
            size_t len = strlen(temp);
            if (len > 7 && chipNameEq(temp + len - 7, ".atanua"))
            {
                char sub[1024];
                const char *slash = strrchr(aPath, '/');
                const char *bslash = strrchr(aPath, '\\');
                if (bslash && (!slash || bslash > slash))
                    slash = bslash;
                if (slash)
                {
                    size_t dirlen = (size_t)(slash - aPath) + 1;
                    if (dirlen + len < sizeof(sub))
                    {
                        memcpy(sub, aPath, dirlen);
                        memcpy(sub + dirlen, temp, len + 1);
                    }
                    else
                        sub[0] = 0;
                }
                else
                {
                    snprintf(sub, sizeof(sub), "%s", temp);
                }
                FILE *boxfh = sub[0] ? atanua_fopen_rb(sub) : NULL;
                if (boxfh)
                    fclose(boxfh);
                else
                {
                    fprintf(out,"ERROR line %d: box file '%s' not found\n",
                           part->GetLineNum(), temp);
                    errors++;
                }
                names.push_back(temp);
                pincounts.push_back(0);
                dynamic_.push_back(1);
                continue;
            }
            int pins = chipPinCount(temp);
            if (pins < 0)
            {
                fprintf(out,"ERROR line %d: unknown chip '%s'\n", part->GetLineNum(), temp);
                errors++;
                names.push_back(temp);
                pincounts.push_back(0);
                dynamic_.push_back(0);
                continue;
            }
            names.push_back(temp);
            pincounts.push_back(pins);
            dynamic_.push_back(0);
        }
        else if (chipNameEq(part->Value(), "Wire"))
        {
            const char *need[4] = { "chip1", "chip2", "pad1", "pad2" };
            int val[4] = { 0, 0, 0, 0 };
            int ok = 1;
            int i;
            for (i = 0; i < 4; i++)
            {
                if (part->QueryIntAttribute(need[i], &val[i]) != tinyxml2::XML_SUCCESS)
                {
                    fprintf(out,"ERROR line %d: Wire without integer %s\n",
                           part->GetLineNum(), need[i]);
                    errors++;
                    ok = 0;
                }
            }
            if (!ok || val[0] < 0 || val[1] < 0 || val[2] < 0 || val[3] < 0)
            {
                if (ok)
                {
                    fprintf(out,"ERROR line %d: Wire with negative index\n", part->GetLineNum());
                    errors++;
                }
                continue;
            }
            if (val[0] >= (int)names.size() || val[1] >= (int)names.size())
            {
                fprintf(out,"ERROR line %d: Wire chip index out of range (%d chips)\n",
                       part->GetLineNum(), (int)names.size());
                errors++;
                continue;
            }
            if (!dynamic_[val[0]] && val[2] >= pincounts[val[0]])
            {
                fprintf(out,"ERROR line %d: pad %d out of range ('%s' has %d pads)\n",
                       part->GetLineNum(), val[2], names[val[0]].c_str(),
                       pincounts[val[0]]);
                errors++;
                continue;
            }
            if (!dynamic_[val[1]] && val[3] >= pincounts[val[1]])
            {
                fprintf(out,"ERROR line %d: pad %d out of range ('%s' has %d pads)\n",
                       part->GetLineNum(), val[3], names[val[1]].c_str(),
                       pincounts[val[1]]);
                errors++;
                continue;
            }
            wires++;
        }
    }

    if (errors)
        return 1;
    fprintf(out, "OK: %d chips, %d wires\n", (int)names.size(), wires);
    return 0;
}

int validate_circuit(const char *aPath)
{
    return validate_circuit_to(aPath, stdout);
}
