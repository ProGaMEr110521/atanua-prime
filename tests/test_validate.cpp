// Drives the shipped headless validator (src/core/validate.cpp) against
// good and broken circuits. Links tinyxml2 from vcpkg (passed as an
// extra linker input by the suite); atanua_fopen_rb is stubbed with
// plain fopen since the suite only feeds ASCII temp paths here — the
// real UTF-8 opener is exercised by the live CLI runs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

FILE *atanua_fopen_rb(const char *aPath)
{
    if (!aPath || !aPath[0])
        return NULL;
    return fopen(aPath, "rb");
}

int validate_circuit_to(const char *aPath, FILE *out);

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } \
} while (0)

static std::string tmpdir()
{
    const char *t = getenv("TEMP");
    if (!t)
        t = getenv("TMP");
    if (!t)
        t = getenv("TMPDIR");
    if (!t)
        t = "/tmp";
    return t;
}

static int casecount = 0;

static std::string run_case(const char *content)
{
    // Write circuit, validate into a sidecar file, read the report back.
    char in[1024], out[1024];
    snprintf(in, sizeof(in), "%s/atanua_val_%d.atanua",
             tmpdir().c_str(), casecount);
    snprintf(out, sizeof(out), "%s/atanua_val_%d.txt",
             tmpdir().c_str(), casecount);
    casecount++;
    FILE *f = fopen(in, "wb");
    if (!f)
    {
        printf("FAIL: cannot write temp file\n");
        failures++;
        return "rc=-1 ";
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    FILE *o = fopen(out, "wb");
    if (!o)
    {
        printf("FAIL: cannot write temp report\n");
        failures++;
        remove(in);
        return "rc=-1 ";
    }
    int rc = validate_circuit_to(in, o);
    fclose(o);
    f = fopen(out, "rb");
    std::string body;
    if (f)
    {
        char buf[1024];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            body.append(buf, n);
        fclose(f);
    }
    remove(in);
    remove(out);
    char code[64];
    snprintf(code, sizeof(code), "rc=%d ", rc);
    return code + body;
}

int main()
{
    std::string o = run_case(
        "<Atanua>\n"
        "<Chip Name=\"logic AND\" xpos=\"251658240\" ypos=\"92274688\" rot=\"0\"/>\n"
        "<Chip Name=\"LED (red)\" xpos=\"293601280\" ypos=\"92274688\" rot=\"0\"/>\n"
        "<Wire chip1=\"0\" pad1=\"2\" chip2=\"1\" pad2=\"0\"/>\n"
        "</Atanua>\n");
    CHECK(o.find("rc=0") == 0, "good circuit exits 0");
    CHECK(o.find("OK: 2 chips, 1 wires") != std::string::npos, "good OK line");

    o = run_case(
        "<Atanua><Chip Name=\"logic ADN\" xpos=\"0\" ypos=\"0\" rot=\"0\"/></Atanua>");
    CHECK(o.find("rc=1") == 0, "unknown chip exits 1");
    CHECK(o.find("unknown chip") != std::string::npos, "unknown chip named");

    o = run_case(
        "<Atanua><Chip Name=\"logic AND\" xpos=\"0\" ypos=\"0\" rot=\"0\"/>"
        "<Chip Name=\"LED (red)\" xpos=\"0\" ypos=\"0\" rot=\"0\"/>"
        "<Wire chip1=\"0\" pad1=\"9\" chip2=\"1\" pad2=\"0\"/></Atanua>");
    CHECK(o.find("rc=1") == 0, "oob pad exits 1");
    CHECK(o.find("out of range") != std::string::npos, "oob pad named");

    o = run_case(
        "<Atanua><Chip Name=\"LED (red)\" xpos=\"0\" ypos=\"0\" rot=\"0\"/>"
        "<Wire chip1=\"5\" pad1=\"0\" chip2=\"0\" pad2=\"0\"/></Atanua>");
    CHECK(o.find("rc=1") == 0, "oob chip exits 1");

    o = run_case(
        "<Atanua><Chip Name=\"LED (red)\" xpos=\"0\" ypos=\"0\" rot=\"0\"/>"
        "<Wire chip1=\"0\" chip2=\"0\" pad2=\"0\"/></Atanua>");
    CHECK(o.find("rc=1") == 0, "missing attr exits 1");
    CHECK(o.find("pad1") != std::string::npos, "missing attr named");

    o = run_case("<Atanua><Chip Name=\"oops\"");
    CHECK(o.find("rc=1") == 0, "malformed xml exits 1");
    CHECK(o.find("XML parse") != std::string::npos, "parse error named");

    o = run_case("<Atanua><Chip xpos=\"0\" ypos=\"0\" rot=\"0\"/></Atanua>");
    CHECK(o.find("rc=1") == 0, "nameless chip exits 1");

    o = run_case("<Design></Design>");
    CHECK(o.find("rc=1") == 0, "missing root exits 1");

    o = run_case(
        "<Atanua><Chip Name=\"missing.atanua\" xpos=\"0\" ypos=\"0\" rot=\"0\"/></Atanua>");
    CHECK(o.find("rc=1") == 0, "missing box exits 1");
    CHECK(o.find("not found") != std::string::npos, "missing box named");

    CHECK(validate_circuit_to("C:\\definitely\\not\\here\\nope.atanua",
                              stdout) == 2, "missing file exits 2");
    CHECK(validate_circuit_to(NULL, stdout) == 2, "null path exits 2");

    if (failures == 0)
        printf("ALL VALIDATE TESTS PASSED\n");
    else
        printf("%d validate failures\n", failures);
    return failures ? 1 : 0;
}
