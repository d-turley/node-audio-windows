export class VolumeControl {
    getVolume(): number;
    setVolume(volume: number): void;
    isMuted(): boolean;
    setMuted(muted: boolean): void;
    execTranslatorMacro(macro: string): void;
}