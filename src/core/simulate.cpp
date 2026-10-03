/* Headless batch simulation for machine consumers (tests, generators).
 *
 * Usage: atanua --simulate <file> [--ticks N] [--set C:P=V ...]
 *   --ticks N     propagation ticks to run (default 100, 1..1000000)
 *   --set C:P=V   force chip C pad P to V (0/1); ideal driver applied
 *                 after every chip-update phase, repeatable
 * Prints one JSON object on stdout, exits 0. Failures print ERROR and
 * exit 1 (unusable circuit) or 2 (misuse). No window, GL, audio, config
 * UI, or prompts are touched; textures hand out dummies with no GL
 * context (see load_texture), chip construction is logic-only.
 * Timing mirrors the app loop: mPhysicsKHz substeps per tick, same
 * virtual clock, same dirty propagation. Wires/chips state is whatever
 * the shipped update() code computes — nothing is reimplemented here.
 */
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <tinyxml2.h>
#include "atanua.h"
#include "atanua_internal.h"
#include "chipdb.h"
#include "basechipfactory.h"
#include "pluginchipfactory.h"
#include "ledchip.h"

extern unsigned char gAudioBuffer[];
extern int gRecordHead;
extern unsigned char *gAudioOut;

static void json_escape(FILE *out, const char *s)
{
    fputc('"', out);
    if (s)
    {
        for (; *s; s++)
        {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\')
            {
                fputc('\\', out);
                fputc(c, out);
            }
            else if (c < 0x20)
                fprintf(out, "\\u%04x", c);
            else
                fputc(c, out);
        }
    }
    fputc('"', out);
}

static const char *netStateName(int s)
{
    switch (s)
    {
    case NETSTATE_HIGH: return "high";
    case NETSTATE_LOW: return "low";
    case NETSTATE_NC: return "nc";
    case NETSTATE_INVALID: return "invalid";
    default: return "unknown";
    }
}

static int netIndex(Pin *pin)
{
    size_t i, k;
    if (!pin || !pin->mNet)
        return -1;
    for (i = 0; i < gNet.size(); i++)
    {
        for (k = 0; k < gNet[i]->mPin.size(); k++)
        {
            if (gNet[i]->mPin[k] == pin)
                return (int)i;
        }
    }
    return -1;
}

struct ForcePin
{
    int chip;
    int pad;
    int value;
};

static int parse_set(const char *spec, ForcePin &out)
{
    int c = -1, p = -1, v = -1;
    if (!spec || sscanf(spec, "%d:%d=%d", &c, &p, &v) != 3)
        return 0;
    if (c < 0 || p < 0 || (v != 0 && v != 1))
        return 0;
    out.chip = c;
    out.pad = p;
    out.value = v;
    return 1;
}

int simulate_circuit(int argc, char **args)
{
    std::string file;
    int ticks = 100;
    std::vector<ForcePin> forces;
    int i;
    for (i = 2; i < argc; i++)
    {
        if (strcmp(args[i], "--ticks") == 0 && i + 1 < argc)
        {
            ticks = atoi(args[++i]);
        }
        else if (strcmp(args[i], "--set") == 0 && i + 1 < argc)
        {
            ForcePin f;
            if (!parse_set(args[++i], f))
            {
                printf("ERROR: bad --set (want chip:pad=value with value 0/1)\n");
                return 2;
            }
            forces.push_back(f);
        }
        else if (args[i][0] == '-' && args[i][1] == '-')
        {
            printf("ERROR: unknown flag '%s'\n", args[i]);
            return 2;
        }
        else if (file.empty())
        {
            file = resolve_argv_path(args[i]);
        }
        else
        {
            printf("ERROR: only one circuit file allowed\n");
            return 2;
        }
    }
    if (file.empty())
    {
        printf("usage: atanua --simulate <file> [--ticks N] [--set C:P=V ...]\n");
        return 2;
    }
    if (ticks < 1)
        ticks = 1;
    if (ticks > 1000000)
        ticks = 1000000;

    FILE *vf = tmpfile();
    if (!vf)
    {
        printf("ERROR: no scratch space\n");
        return 2;
    }
    int vrc = validate_circuit_to(file.c_str(), vf);
    if (vrc != 0)
    {
        rewind(vf);
        int ch;
        while ((ch = fgetc(vf)) != EOF)
            fputc(ch, stdout);
        fclose(vf);
        return 1;
    }
    fclose(vf);

    gConfig.load();
    gChipFactory.push_back(new BaseChipFactory);
    gChipFactory.push_back(new PluginChipFactory);
    for (i = 0; i < (signed)gChipFactory.size(); i++)
        gChipFactory[i]->getSupportedChips(gAvailableChip);

    // Box-heavy files would prompt in the GUI loader; refuse instead of
    // ever hanging headless.
    {
        int boxes = 0;
        tinyxml2::XMLDocument doc;
        FILE *fh = atanua_fopen_rb(file.c_str());
        if (fh && doc.LoadFile(fh) == tinyxml2::XML_SUCCESS)
        {
            tinyxml2::XMLElement *root = doc.FirstChildElement("Atanua");
            if (root)
            {
                for (tinyxml2::XMLElement *part = root->FirstChildElement();
                     part != 0; part = part->NextSiblingElement())
                {
                    if (chipNameEq(part->Value(), "Chip"))
                    {
                        const char *nm = part->Attribute("Name");
                        if (nm)
                        {
                            size_t ln = strlen(nm);
                            if (ln > 7 && chipNameEq(nm + ln - 7, ".atanua"))
                                boxes++;
                        }
                    }
                }
            }
        }
        if (fh)
            fclose(fh);
        if (boxes > gConfig.mMaxActiveBoxes)
        {
            printf("ERROR: %d boxes exceed the headless limit %d\n",
                   boxes, gConfig.mMaxActiveBoxes);
            return 1;
        }
    }

    do_loaddialog(0, file.c_str());
    do_build_nets();

    for (i = 0; i < (signed)forces.size(); i++)
    {
        if (forces[i].chip >= (int)gChip.size() ||
            forces[i].pad >= (int)gChip[forces[i].chip]->mPin.size())
        {
            printf("ERROR: --set target %d:%d not present\n",
                   forces[i].chip, forces[i].pad);
            return 1;
        }
    }

    gAudioOut = gAudioBuffer;
    float physicstick = 0.0f;
    int khz = gConfig.mPhysicsKHz > 0 ? gConfig.mPhysicsKHz : 1;
    int t, k;
    size_t n;
    for (t = 0; t < ticks; t++)
    {
        for (k = 0; k < khz; k++)
        {
            for (n = 0; n < gChip.size(); n++)
            {
                if (gChip[n]->mDirty)
                {
                    gChip[n]->mDirty = 0;
                    gChip[n]->update(physicstick);
                }
            }
            for (i = 0; i < (signed)forces.size(); i++)
            {
                Pin *pin = gChip[forces[i].chip]->mPin[forces[i].pad];
                pin->setState(forces[i].value ? PINSTATE_WRITE_HIGH
                                              : PINSTATE_WRITE_LOW);
            }
            for (n = 0; n < gNet.size(); n++)
            {
                if (gNet[n]->mDirty)
                {
                    gNet[n]->mDirty = 0;
                    gNet[n]->update();
                }
                else
                {
                    gNet[n]->mHighFreqChanges = 0;
                }
            }
            physicstick += 1.0f / khz;
        }
    }

    printf("{\"file\":");
    json_escape(stdout, file.c_str());
    printf(",\"ticks\":%d,\"chips\":%d,\"wires\":%d,\"wireLegacy\":%d,\"leds\":[",
           ticks, (int)gChip.size(), (int)gWire.size(), gConfig.mWireLegacy ? 1 : 0);
    int first = 1;
    for (n = 0; n < gChip.size(); n++)
    {
        if (!dynamic_cast<LEDChip *>(gChip[n]) || gChip[n]->mPin.size() < 1)
            continue;
        Pin *pin = gChip[n]->mPin[0];
        const char *st = "unconnected";
        if (pin->mNet)
            st = netStateName(pin->mNet->mState);
        if (!first)
            printf(",");
        first = 0;
        printf("{\"chip\":%d,\"name\":", (int)n);
        json_escape(stdout, gChipName[n]);
        printf(",\"state\":\"%s\"}", st);
    }
    printf("],\"nets\":[");
    first = 1;
    for (n = 0; n < gNet.size(); n++)
    {
        if (!first)
            printf(",");
        first = 0;
        printf("{\"net\":%d,\"state\":\"%s\",\"pins\":%d}", (int)n,
               netStateName(gNet[n]->mState), (int)gNet[n]->mPin.size());
    }
    printf("],\"pins\":[");
    first = 1;
    for (n = 0; n < gChip.size(); n++)
    {
        size_t p;
        for (p = 0; p < gChip[n]->mPin.size(); p++)
        {
            Pin *pin = gChip[n]->mPin[p];
            if (!first)
                printf(",");
            first = 0;
            if (pin->mNet)
                printf("{\"chip\":%d,\"pad\":%d,\"net\":%d,\"state\":\"%s\"}",
                       (int)n, (int)p, netIndex(pin),
                       netStateName(pin->mNet->mState));
            else
                printf("{\"chip\":%d,\"pad\":%d,\"net\":-1,\"state\":\"unconnected\"}",
                       (int)n, (int)p);
        }
    }
    printf("]}\n");
    return 0;
}
