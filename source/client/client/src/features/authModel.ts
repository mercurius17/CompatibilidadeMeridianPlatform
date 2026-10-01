export interface RemoteAuthGameData {
  session: string;
  masterApiId: number;
  discordUsername: string | null;
  discordDiscriminator: string | null;
  discordAvatar: string | null;
};

export interface LocalAuthGameData {
  profileId: number;
};

export interface CredentialsAuthGameData {
  username: string;
  password: string;
};

export interface AuthGameData {
  remote?: RemoteAuthGameData;
  local?: LocalAuthGameData;
  credentials?: CredentialsAuthGameData;
  token?: string;              // <-- NOVO: token vindo do launcher via env var
};

export const authGameDataStorageKey = "authGameData";
