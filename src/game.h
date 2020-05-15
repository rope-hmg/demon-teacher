enum TileType {
    OuterFenceVertical,
    OuterFenceHorizontal,
    OuterGatePost,
    BalconyVertical,
    BalconyHorizontal,
    StairsUp,
    StairsDown,
    RunningTrackVertical,
    RunningTrackHorizontal,
    RunningTrackCurveA, // TODO(Hector): Think about how this should work.
    HallwayFloor,
    ClassRoomFloor,
    LibraryFloor,
    DirtFloor,
    GrassFloor,
    PentagramTL,
    PentagramTM,
    PentagramTR,
    PentagramML,
    PentagramMM,
    PentagramMR,
    PentagramBL,
    PentagramBM,
    PentagramBR,
    StreetFloor0,
    StreetFloor1,
};

struct TileMap {
         u32       width;
         u32       height;
    enum TileType* tiles;
};

struct GameState {
           bool    initialised;
           u32     x_offset;
           u32     y_offset;
    struct Locale* locale;
};
