#pragma once

class PIDStudio;
class PIDFile;
class PIDPalette;

enum BATCH_TYPE {
    BATCH_SAVE_AS_PNG,
    BATCH_SAVE_AS_PID,
    BATCH_TRANSFORM_PALETTE,
    BATCH_CREATOR_PROCESS,
    BATCH_IMPORT_PNGS
};

namespace Batch {
    void setInPalette(const std::shared_ptr<PIDPalette>&);
    void setOutPalette(const std::shared_ptr<PIDPalette>&);
    void setExportDirectory(const std::filesystem::path&);
    void openImportPNGSSettings(PIDStudio*, const std::filesystem::path& pathOut);
    void importPNGSSettings(PIDStudio*);
    void openCreator(PIDStudio*, const std::filesystem::path&);
    void creator(PIDStudio*);
    void start(const std::filesystem::path&, BATCH_TYPE);
    void start(const std::vector<std::filesystem::path>&, BATCH_TYPE);
    void batch(PIDStudio*);
    void processSingleFile(PIDStudio*);
    bool importSinglePNG(PIDStudio*, const std::filesystem::path&);
};