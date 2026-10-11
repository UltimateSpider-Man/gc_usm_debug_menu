# Ultimate Spider-Man Debug Menu — GameCube

Port freestanding PowerPC del debug menu PC per la revisione NTSC-U del
`main.dol` incluso nel repository.

Il payload usa gli indirizzi ricostruiti da `gc_address.txt` e verificati contro
le istruzioni del DOL reale. Viene caricato a `0x80636040`, prima dell'ArenaLo.
Un hook disegna immediatamente prima di `nglListEndScene`, un secondo esegue gli
handler differiti alla fine di `game::frame_advance` e un terzo assegna al menu
il focus esclusivo del pad.

## Build e iniezione

Requisiti: devkitPPC e Python 3, oppure WSL con Python 3 e un compilatore GNU
PowerPC con supporto EABI. Da PowerShell:

```powershell
.\build_gamecube.ps1
```

Il risultato riproducibile è `build/gamecube/main_patched.dol`; il DOL sorgente
viene sempre salvato come backup verificato prima di qualsiasi sostituzione.
Dettagli e parametri sono in [README_GAMECUBE.md](README_GAMECUBE.md).

## Controlli

- `Z`: apre/chiude il menu; è l'unico pulsante di attivazione
- D-Pad o stick sinistro su/giù: selezione
- D-Pad o stick sinistro sinistra/destra: modifica il valore
- `A`: entra/esegue/commuta
- `B`: torna indietro
- `X` tenuto: variazione rapida per i valori nativi

Il pulsante Z e i fronti D-Pad/A/B vengono acquisiti direttamente dall'hook
`PADRead`, restano memorizzati fino al frame di rendering e funzionano da
qualunque porta pad. Il
controller che apre il menu ne mantiene il controllo finché resta connesso.
Quando il menu possiede la pausa, la chiusura usa anche il Deactivate nativo
del pause menu per consentire la ripresa dopo un dialogo comparso durante
la pausa debug. Le pause già attive prima dell'apertura restano del gioco.

## Copertura

Sono collegati agli handler GameCube: Camera con le scelte esplicite Chase,
User e Scene Analyzer, Warp per regione e Point of
Interest, fisica, single step, slow motion, Spider Monkey, rumble, God Mode,
visualizzazione debug/FPS, Save/Load/Auto Load con slot, callback screenshot, tutti i 151
flag e 78 interi Devopts, tutti i 97 campi Saved Game Settings nel layout GC a 32 bit, District
variants, unload/riattivazione dello streamer distretti e i 51 Debug Render
item. Sono inoltre dinamici: scaricamento e avvio Missions, enumerazione e
caricamento Level, Hero Select filtrato sui pack disponibili, elenco AI
high-priority con parametri core/inode modificabili, contatori Slabs, inventario
e playback delle animazioni, Entity Variants e Kill Hero. Sono operativi anche
Controller 2/mouselook, inversione della camera e Reboot tramite OSResetSystem.
Le liste AI, inode, actor, clip e varianti hanno controlli di pagina.
AI usa la lista nativa high-priority corretta e apre i parametri del core e
degli inode; i valori numerici si modificano con sinistra/destra. L'azione
`--Export this block--` salva tutti i parametri del blocco tramite il
companion in `build/ai-exports`, senza dipendere dal file server retail.
Entity Animations mantiene gli attori validi anche quando la ricerca include
oggetti statici ARAM e verifica banca, compatibilità e ownership prima del
playback nativo, ripristinando la velocità a 1.0.
La riga Camera mostra `Camera : chase`, `Camera : user` oppure
`Camera : sceneanalyzer`; A e sinistra/destra cambiano modalità.
User sincronizza anche il flag nativo di abilitazione, evitando il ritorno
automatico a Chase nel frame successivo.

I controlli Game e Devopts leggono i valori correnti del gioco: Physics,
Slow Motion e God Mode restano sincronizzati tra i menu. GOD_MODE applica
subito i cheat nativi; CAMERA_STATE e Controller 2 usano i rispettivi
handler. Show FPS, Show Hero Position e Show Districts disegnano overlay
del payload, anche quando i renderer debug retail sono stati rimossi.
I flag e gli interi generici conservano nomi e layout nativi GameCube:
alcune opzioni vengono consumate solo al caricamento o hanno il consumer
rimosso nella versione retail, quindi modificarle non crea tale funzionalità.

Replay compare subito dopo District variants. Il recorder RTDT del sorgente
PC fornito è vuoto; la registrazione GameCube viene implementata nel payload.
`Record` registra gli input reali dei quattro controller, `Play` li riproduce,
`Stop` conserva la registrazione e `Clear` la cancella. Z arresta registrazione
e playback e mantiene il controllo del menu. Il buffer contiene al massimo
3600 aggiornamenti di gioco; Samples / Duration mostra quantità e tempo di gioco.
I poll multipli dei controller nello stesso aggiornamento usano un unico sample.
La registrazione vive in RAM e scade cambiando world, terrain o eroe.
Il playback parte dalla posizione attuale e ripete gli input: non ripristina
posizione iniziale, AI, missioni, RNG o stato fisico del mondo.
Abilitare Physics, scegliere Record o Play e rilasciare A. Record sostituisce
la registrazione precedente; mantenere connesse le porte controller registrate.
Le pause native e i dialoghi sospendono il playback: chiudere normalmente
il dialogo per continuare.

Hero Select esegue remove/add fuori dal render, ripara l'HUD tramite gli
entrypoint REL verificati e tenta di ripristinare l'eroe precedente se il nuovo
pack non crea il player. Durante questa transazione navigazione e chiusura sono
bloccate, così nessun altro handler cambia contesto e il gioco non riparte mai
nel tratto intermedio senza player.
`ShowPerfBar` e `ScreenShot` usano i due flag NGL nativi verificati.
`ShowPerfInfo` (valore corrente/media/picco) e `ShowPerfGraph` (120 frame)
sono overlay del payload alimentati dal timer CPU NGL nativo. Le altre 18
voci PC restano `N/A`: non è stato verificato un controllo retail equivalente.

`Progression` ora intercetta i binding VM nativi: registra fino a 512 voci
create dagli script, esegue i loro handler reali e supporta getter/setter,
limiti numerici, rimozione e invalidazione alla distruzione dell'istanza.
Se non ci sono voci, può invocare `setup_progression_debug_menu()` quando la
funzione è presente negli script caricati.

`Missions` enumera le missioni di Spider-Man e Venom per distretto, altre
missioni e cinematiche. Scarica la missione corrente, cambia eroe quando
necessario e avvia gli identificatori originali; le transazioni bloccano altre
azioni fino al completamento. Fisica disabilitata e risorse mancanti producono
un messaggio esplicito.

`Script` include `Pop Character`, `Pop All Characters`, `Spawn All Characters`
e tutti i 66 nomi richiesti. Lo spawn è distanziato nel tempo e verifica pack,
classe, memoria, handle e ownership. La coda e gli attori vengono conservati
durante lo stato nativo temporaneo di ritorno dalla pausa, evitando la
cancellazione del primo spawn. Le nuove richieste acquisiscono subito il world
e il terrain correnti. Le transizioni di missione, livello ed
eroe rimuovono prima gli attori e i pack posseduti dal menu. Nella copia
fornita ci sono 38 coppie esatte e 25 mapping verificati, mostrati nella riga;
`venom_spider_viewer`, `boss_venom_spider` e `gang_skin_boss_fem` non hanno
asset corrispondenti. L'inventario è in `build/gamecube/character-assets.json`.

`Level Select` mostra `city` e `characterb`, corrispondenti a `city_arena` e
`characterb_arena`. City salva `SCENE_NAME=city_arena` tramite il companion e
riavvia il disco solo dopo la conferma di scrittura di INI e ISO. In assenza
del companion, su errore o timeout non riavvia e restituisce il controllo.
Characterb aggiorna lo `SCENE_NAME` nativo prima del caricamento e pubblica
una richiesta di persistenza. `CHARACTERB_ARENA` non è
presente nel disco fornito: la riga resta visibile, ma non avvia né salva una
scena senza asset. Gli altri livelli vengono mostrati e caricati solo quando
il relativo pack è disponibile; nella copia fornita è presente solo City.

MemTrack e Script Memtrack dichiarano gli stub rimossi.
Le callback screenshot native sono collegate, ma il salvataggio TGA non è
operativo: `PCcreat` e `PCopen` restituiscono sempre errore nel DOL retail.
Il menu PC Dvars con sei valori non è incluso: l'aggiornamento retail è uno
stub e la camera usa costanti, senza globali modificabili equivalenti verificati.
Queste limitazioni impediscono di dichiarare parità completa con il menu PC.

## Salvataggio di game.ini con Dolphin

Il disco GameCube montato è di sola lettura. Per salvare la selezione nel
`game.ini` fornito e nella copia ISO usata da Dolphin, avviare:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\launch_debug_dolphin.ps1
```

Lo script usa i percorsi locali forniti, crea `build/debug-disc/usm-debug-game-replay.iso`
con il nuovo `main.dol` e un'area riservata al file INI, poi avvia Dolphin e il
companion Windows. Lasciare il companion aperto. I parametri `-SourceIso`,
`-GameIni`, `-Dolphin`, `-Python` e `-UserDirectory` permettono altri percorsi.
Il disco sorgente è conservato; il primo cambiamento dell'INI crea
`game.ini.usm-debug-original`. Commenti e altre impostazioni restano intatti.
La riga `game.ini SCENE_NAME` distingue `Saved`, errore e companion assente.
City richiede il companion per salvare e riavviare automaticamente; questo
percorso di persistenza è disponibile in Dolphin su Windows.

La build e l'iniezione sono validate con controlli automatici su input, coda
differita, ownership, layout GC e file DOL. I test host sostituiscono il confine
engine con fixture MEM1; non dimostrano il comportamento completo in gioco.
Le modifiche scalari con handler sono applicate insieme al callback nel frame
di aggiornamento, così chiusura e blocco della transazione annullano anche le
scritture ancora in coda.
Le suite host coprono anche Progression, tutte le righe personaggio, cleanup,
cambio eroe nelle missioni, alias scena e mailbox. Il companion è stato
provato con scritture e conferme reali a un INI di prova e a una ISO montata
in Dolphin. La prova interattiva completa degli handler rimane necessaria;
vedere `build/gamecube/verification.json` per evidenza e limiti esatti.

## Revisione supportata

SHA-256 del DOL originale:

`3dd0ad5ede2ef9df27a1aba1be9bf9cd5dfd7a14caa4bb8da396f2574c89565c`

L'iniettore rifiuta revisioni, opcode o layout differenti anziché applicare uno
scarto globale non valido.

## Licenza

MIT, come il progetto originale.
