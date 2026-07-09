// moduleMatches = 0x6267BFD0

extern "C" {

__attribute__((used)) const char gPatchCompilerSmokeName[] = "patch-smoke";
__attribute__((used)) int gPatchCompilerSmokeData[4] = {1, 2, 3, 4};

__attribute__((used)) int patchCompilerSmokeTest(int seed) {
    int result = 0;
    for (int i = 0; i < 4; ++i) {
        result += gPatchCompilerSmokeData[i] * (seed + i);
    }
    return result;
}

}
