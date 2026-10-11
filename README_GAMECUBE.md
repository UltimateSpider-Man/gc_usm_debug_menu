# Build e iniezione GameCube

Il payload viene collegato a `0x80636040` e aggiunto allo slot testo DOL `3`.
L'iniettore applica 26 patch con word originale verificata:

- wrapper ArenaLo ai due call-site `0x802F39B8` e `0x802F39F4`;
- render/menu hook a `0x8016E55C`;
- coda handler/Hero Select a `0x8017BEE0`, nel tail di
  `game::frame_advance`;
- filtro input al call-site `PADRead` `0x801ACCC8`.

Il manifest `tools/progression_bindings.json` aggiunge 20 puntatori assoluti
nelle vtable dei binding VM Progression e una chiamata di cleanup alla
distruzione dell'istanza script. I puntatori fisici del DOL diventano puntatori
MEM1 al payload; i binding restano validi durante la paginazione ARAM nativa.

Il filtro conserva l'input grezzo per il debug menu, rileva solo il pulsante
`Z` e memorizza i fronti di attivazione, D-Pad, A e
B direttamente nell'hook `PADRead`, così una pressione breve non si perde prima
del render. Lo stick sinistro usa soglia e isteresi come navigazione alternativa.
Tutte e quattro le porte possono aprire il menu; la porta che lo apre mantiene
il focus. Il filtro neutralizza i controlli consegnati al gioco mentre il menu
è visibile e fino al completamento dell'eventuale unpause differito. Il menu abilita gli handler
di gameplay solo dopo che stato `PAUSED` e latch `game_paused` confermano la
pausa; durante remove/add di Hero Select blocca anche navigazione e chiusura.
Lo script di build non sovrascrive il DOL usato come input; crea:

- `build/gamecube/usm_debug_menu.elf`, `.bin`, `.map` e `.symbols.txt`;
- `build/gamecube/main_original.dol`, backup verificato dell'originale;
- `build/gamecube/main_patched.dol`, DOL avviabile con il payload.

Nel pacchetto già iniettato, `main.dol` è il risultato finale e
`main.original.dol` è la copia recuperabile. Se quest'ultima esiste, lo script
la sceglie automaticamente come input per consentire build ripetibili.

## Requisiti

- devkitPPC (`DEVKITPPC`, `DEVKITPRO` oppure installazione in
  `C:\devkitPro\devkitPPC`);
- Python 3; in alternativa ai tool Windows, WSL con Python 3 e devkitPPC
  oppure `g++-powerpc-linux-gnu`;
- la revisione `main.dol` inclusa nel repository, SHA-256
  `3dd0ad5ede2ef9df27a1aba1be9bf9cd5dfd7a14caa4bb8da396f2574c89565c`.

Eseguire da PowerShell:

```powershell
.\build_gamecube.ps1
```

Se devkitPPC Windows non è installato, lo script usa `build_gamecube.sh` in
WSL. Il compilatore GNU viene configurato esplicitamente per PPC750 EABI,
big-endian, hard-float e senza startup/librerie Linux. I sorgenti sono nella
cartella `gamecube`; è supportato anche Windows PowerShell 5.1.

Per aggiornare esplicitamente il DOL del pacchetto conservando l'originale:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_gamecube.ps1 -OutputDol main.dol
```

`-DryRun` compila e valida l'iniezione senza scrivere backup o DOL finale.
Percorsi e SHA atteso sono configurabili tramite i parametri dello script; una
revisione diversa deve sempre fornire esplicitamente il proprio hash esatto.

Il renderer replica il layout PC compatto: pannello scuro dimensionato al
testo, titolo giallo, riga selezionata verde, valori inline e pagine da 18
righe con indicatori di scorrimento magenta.

## Contratto del runtime

I sorgenti sotto `gamecube` devono esportare con linkage C:

```cpp
extern "C" void usm_debug_menu_hook();
extern "C" void usm_debug_menu_update_hook(void *, float);
extern "C" unsigned int usm_debug_menu_pad_read_hook(usm::gc::PadStatus *);
extern "C" unsigned char __payload_end[];
```

Il wrapper assembly `usm_gc_arena_lo_wrapper` vive in
`.text.arena_wrapper`; il linker lo mantiene come prima funzione del payload.
Inoltra a `OSSetArenaLo` usando almeno `__payload_end`; l'hook sostituisce la chiamata originale a
`0x8016E55C` e deve conservarne il comportamento richiesto.

L'iniettore accetta direttamente ELF32 PowerPC big-endian e appiattisce tutti i
segmenti `PT_LOAD`, inclusi gap e BSS azzerati fino a `__payload_end`. Supporta
anche un payload raw con `--payload-format binary --symbols <output nm>`.

Prima di scrivere valida hash, tutte le 26 word originali, slot libero,
sovrapposizioni, limiti MEM1 e range dei branch PPC.

## Verifica

Da WSL, nella cartella del port:

```sh
bash tools/run_runtime_checks.sh
python3 -m unittest discover -s tools/tests -p 'test_*.py' -v
```

La seconda suite richiede una build ELF fresca. Le suite host verificano il
codice reale con memoria MEM1 simulata e API engine sostituite al confine.
Coprono attivazione esclusiva con Z, focus pad, pausa, cancellazione della coda,
layout Saved Settings, AI/animazioni/varianti, ownership missioni e iniezione
negativa/ripetibile. Non sostituiscono una prova di tutti gli handler in gioco.

Le suite includono runtime/input, diagnostica, layout Saved Settings,
missioni di entrambi gli eroi, Progression VM, ciclo di vita degli spawn,
scene/mailbox, conferma del salvataggio prima del reboot, Single Step nativo
e le scelte Camera Chase/User/Scene Analyzer, inclusa la persistenza di User.
Game/Devopts copre tutti i 151 flag e 78 interi, gli effetti immediati di
God Mode, lo stato corrente di Physics, il ripristino di Slow Motion e gli
overlay FPS/posizione/distretti. Lo spawn conserva la coda e gli attori durante
il ritorno nativo dalla pausa; i test distinguono tale stato dai veri unload.
Replay verifica registrazione e playback degli input, un solo sample per
aggiornamento anche con più poll, controllo Z fuori dalla registrazione,
disconnect, ownership, limiti del buffer e durata. Il replay non è uno
snapshot del mondo: parte dalla posizione corrente e non riavvolge AI o missioni.
Le risorse ARAM paginate vengono lette soltanto entro gli intervalli di pack
READY ancora posseduti dalle partizioni native; gli oggetti modificabili
restano limitati a MEM1, salvo il comicpanel UI nativo verificato, che usa
anche il pager ARAM durante Deactivate. I test Python coprono anche INI, FST,
protezione dei file originali, reset del companion, conferme e scritture
negative. Il resoconto aggiornato con hash e limiti è in
`build/gamecube/verification.json`.

Per una copia avviabile con persistenza `SCENE_NAME`, eseguire
`launch_debug_dolphin.ps1`. Usa `tools/debug_disc.py` per aggiungere DOL e INI
in una copia workspace e `tools/dolphin_ini_companion.py` per salvare i cambi
richiesti dalla mailbox. Il companion Windows richiede Python a 64 bit.
Lo stesso companion salva gli snapshot AI in `build/ai-exports`; l'azione
`--Export this block--` esporta l'intero blocco, comprese le altre pagine,
e segnala errore se supera il limite di 64 KiB.
Il disco originale resta intatto; il primo cambiamento del file INI conserva
`game.ini.usm-debug-original`. Se un'immagine sorgente è già piena fino al
limite fisico GameCube, la creazione della copia viene rifiutata: occorre
prima ricostruire un'immagine con spazio disponibile.
