import { Settings } from "./settings";
import * as crc32 from "crc-32";
import * as path from "path";
import * as fs from "fs";

export type PluginKind = "RegularEsp" | "Master" | "LightEsl" | "LightEspfe";

export interface ManifestModEntry {
  filename: string;
  crc32: number;
  size: number;
  isLight: boolean;
  isMaster: boolean;
  kind: PluginKind;
}

export interface Manifest {
  versionMajor: number;
  mods: Array<ManifestModEntry>;
  loadOrder: Array<string>;
}

const RECORD_FLAG_ESM = 0x00000001; // Bit 0 do cabeçalho TES4
const RECORD_FLAG_LIGHT = 0x00000200; // Bit 9 do cabeçalho TES4 (ESL / ESPFE)

export const readPluginHeaderInfo = (
  filePath: string,
  existingBuf?: Uint8Array
): { isLight: boolean; isMaster: boolean; kind: PluginKind; flags: number } => {
  let buf: Uint8Array;
  if (existingBuf && existingBuf.length >= 12) {
    buf = existingBuf;
  } else {
    const fd = fs.openSync(filePath, "r");
    try {
      const headerBuf = Buffer.alloc(12);
      const bytesRead = fs.readSync(fd, headerBuf, 0, 12, 0);
      if (bytesRead < 12) {
        throw new Error(
          `Arquivo corrompido ou truncado (menos de 12 bytes): '${filePath}'`
        );
      }
      buf = headerBuf;
    } finally {
      fs.closeSync(fd);
    }
  }

  const sig = String.fromCharCode(buf[0], buf[1], buf[2], buf[3]);
  if (sig !== "TES4") {
    throw new Error(
      `Assinatura de plugin inválida: '${sig}' (esperado 'TES4') em '${filePath}'`
    );
  }

  const flags =
    (buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24)) >>> 0;

  const ext = path.extname(filePath).toLowerCase();
  const hasLightFlag = (flags & RECORD_FLAG_LIGHT) !== 0;
  const hasEsmFlag = (flags & RECORD_FLAG_ESM) !== 0;

  let kind: PluginKind;
  if (ext === ".esl") {
    kind = "LightEsl";
  } else if (hasLightFlag) {
    kind = "LightEspfe";
  } else if (ext === ".esm" || hasEsmFlag) {
    kind = "Master";
  } else {
    kind = "RegularEsp";
  }

  return {
    isLight: kind === "LightEsl" || kind === "LightEspfe",
    isMaster: kind === "Master" || hasEsmFlag,
    kind,
    flags,
  };
};

export const getBsaNameByEspmName = (espmName: string): string => {
  const ext = path.extname(espmName).toLowerCase();
  if (ext === ".esp" || ext === ".esm" || ext === ".esl") {
    const nameNoExt = espmName.slice(0, -ext.length);
    return nameNoExt + ".bsa";
  }
  throw new Error(`'${espmName}' is not a valid esp, esm, or esl name`);
};

export const generateManifest = (settings: Settings): void => {
  const manifest: Manifest = {
    mods: [],
    versionMajor: 1,
    loadOrder: settings.loadOrder.map((x) => path.basename(x)),
  };

  settings.loadOrder.forEach((loadOrderElement) => {
    const espmName = path.isAbsolute(loadOrderElement)
      ? path.basename(loadOrderElement)
      : loadOrderElement;

    const espmPath = path.isAbsolute(loadOrderElement)
      ? loadOrderElement
      : path.join(settings.dataDir, espmName);

    const buf: Uint8Array = fs.readFileSync(espmPath);
    const headerInfo = readPluginHeaderInfo(espmPath, buf);

    manifest.mods.push({
      crc32: crc32.buf(buf),
      filename: espmName,
      size: buf.length,
      isLight: headerInfo.isLight,
      isMaster: headerInfo.isMaster,
      kind: headerInfo.kind,
    });

    const bsaName = getBsaNameByEspmName(espmName);
    const bsaPath = path.join(settings.dataDir, bsaName);
    if (fs.existsSync(bsaPath)) {
      const buf: Uint8Array = fs.readFileSync(bsaPath);
      manifest.mods.push({
        crc32: crc32.buf(buf),
        filename: bsaName,
        size: buf.length,
        isLight: false,
        isMaster: false,
        kind: "RegularEsp",
      });
    }
  });

  const manifestPath = path.join(settings.dataDir, "manifest.json");
  fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 4));
};