// ==============================================
// Internationalisation
// ==============================================

struct Locale {
    char* title;
};

// TODO(Hector):
// Move these into files and load them at runtime.

static struct Locale en_gb = {
    .title = "Demon Teacher",
};

static struct Locale zh_tw = {
    .title = "鬼老師",
};
