export type PluginKind = "RegularEsp" | "Master" | "LightEsl" | "LightEspfe";

export interface Mod {
    filename: string;
    size: number;
    crc32: number;
    isLight?: boolean;
    isMaster?: boolean;
    kind?: PluginKind;
};

export interface ServerManifest {
    versionMajor: number;
    mods: Mod[];
    loadOrder: string[];
};
