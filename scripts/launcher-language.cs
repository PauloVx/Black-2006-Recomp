using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace BlackXboxLauncher
{
    internal static class LauncherLanguage
    {
        private static string current = GameLanguageBanks.English;
        private static readonly object languageCacheLock = new object();
        private static readonly System.Lazy<Dictionary<string, Dictionary<string, string>>> generatedCatalogs =
            new System.Lazy<Dictionary<string, Dictionary<string, string>>>(LoadGeneratedCatalogs);
        private static readonly Dictionary<string, Dictionary<string, string>> translationsByLanguage =
            new Dictionary<string, Dictionary<string, string>>(System.StringComparer.Ordinal);
        private static readonly Dictionary<string, Regex> matchersByLanguage =
            new Dictionary<string, Regex>(System.StringComparer.Ordinal);

        private static Dictionary<string, Dictionary<string, string>> GeneratedCatalogs
        {
            get { return generatedCatalogs.Value; }
        }

        private static Dictionary<string, Dictionary<string, string>> LoadGeneratedCatalogs()
        {
            var catalogs = new Dictionary<string, Dictionary<string, string>>();
            try
            {
                using (Stream stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("BlackXboxLauncher.launcher-locales.json"))
                {
                    if (stream == null) return catalogs;
                    using (StreamReader reader = new StreamReader(stream))
                    {
                        var serializer = new JavaScriptSerializer();
                        var loaded = serializer.Deserialize<Dictionary<string, Dictionary<string, string>>>(reader.ReadToEnd());
                        if (loaded != null) catalogs = loaded;
                    }
                }
            }
            catch (System.InvalidOperationException) { }
            catch (IOException) { }
            return catalogs;
        }
        private static readonly Dictionary<string, string> spanish = new Dictionary<string, string>
        {
            { "Display", "Pantalla" }, { "Graphics", "Gráficos" }, { "Input", "Entrada" }, { "Game", "Juego" },
            { "Menu language", "Idioma del menú" }, { "Game menu language", "Idioma del juego" },
            { "Select a language", "Selecciona un idioma" }, { "English", "Inglés" },
            { "Português (Brasil)", "Portugués (Brasil)" }, { "Spanish", "Español" }, { "French", "Francés" },
            { "German", "Alemán" }, { "Italian", "Italiano" }, { "Ready to play", "Listo para jugar" },
            { "Open logs", "Abrir registros" }, { "Send error", "Enviar error" }, { "Stop game", "Detener juego" },
            { "PLAY", "JUGAR" }, { "Resolution", "Resolución" }, { "Window mode", "Modo de ventana" },
            { "Internal resolution", "Resolución interna" }, { "Scaling", "Escalado" },
            { "Scaling filter", "Filtro de escalado" }, { "VSync", "Sincronización vertical" },
            { "Frame limit", "Límite de FPS" }, { "Camera aspect", "Relación de aspecto" },
            { "Field of view", "Campo de visión" }, { "Controls", "Controles" },
            { "Keyboard and mouse", "Teclado y ratón" }, { "Controller", "Mando" }
        };
        private static readonly Dictionary<string, string> french = new Dictionary<string, string>
        {
            { "Display", "Affichage" }, { "Graphics", "Graphismes" }, { "Input", "Entrée" }, { "Game", "Jeu" },
            { "Menu language", "Langue du menu" }, { "Game menu language", "Langue du jeu" },
            { "Select a language", "Sélectionnez une langue" }, { "English", "Anglais" },
            { "Português (Brasil)", "Portugais (Brésil)" }, { "Spanish", "Espagnol" }, { "French", "Français" },
            { "German", "Allemand" }, { "Italian", "Italien" }, { "Ready to play", "Prêt à jouer" },
            { "Open logs", "Ouvrir les journaux" }, { "Send error", "Signaler une erreur" }, { "Stop game", "Arrêter le jeu" },
            { "PLAY", "JOUER" }, { "Resolution", "Résolution" }, { "Window mode", "Mode fenêtre" },
            { "Internal resolution", "Résolution interne" }, { "Scaling", "Mise à l’échelle" },
            { "Scaling filter", "Filtre de mise à l’échelle" }, { "VSync", "Synchronisation verticale" },
            { "Frame limit", "Limite d’images/s" }, { "Camera aspect", "Format d’image" },
            { "Field of view", "Champ de vision" }, { "Controls", "Commandes" },
            { "Keyboard and mouse", "Clavier et souris" }, { "Controller", "Manette" }
        };
        private static readonly Dictionary<string, string> german = new Dictionary<string, string>
        {
            { "Display", "Anzeige" }, { "Graphics", "Grafik" }, { "Input", "Eingabe" }, { "Game", "Spiel" },
            { "Menu language", "Menüsprache" }, { "Game menu language", "Spielsprache" },
            { "Select a language", "Sprache auswählen" }, { "English", "Englisch" },
            { "Português (Brasil)", "Portugiesisch (Brasilien)" }, { "Spanish", "Spanisch" }, { "French", "Französisch" },
            { "German", "Deutsch" }, { "Italian", "Italienisch" }, { "Ready to play", "Spielbereit" },
            { "Open logs", "Protokolle öffnen" }, { "Send error", "Fehler melden" }, { "Stop game", "Spiel beenden" },
            { "PLAY", "SPIELEN" }, { "Resolution", "Auflösung" }, { "Window mode", "Fenstermodus" },
            { "Internal resolution", "Interne Auflösung" }, { "Scaling", "Skalierung" },
            { "Scaling filter", "Skalierungsfilter" }, { "VSync", "Vertikale Synchronisierung" },
            { "Frame limit", "FPS-Begrenzung" }, { "Camera aspect", "Seitenverhältnis" },
            { "Field of view", "Sichtfeld" }, { "Controls", "Steuerung" },
            { "Keyboard and mouse", "Tastatur und Maus" }, { "Controller", "Controller" }
        };
        private static readonly Dictionary<string, string> italian = new Dictionary<string, string>
        {
            { "Display", "Schermo" }, { "Graphics", "Grafica" }, { "Input", "Input" }, { "Game", "Gioco" },
            { "Menu language", "Lingua del menu" }, { "Game menu language", "Lingua del gioco" },
            { "Select a language", "Seleziona una lingua" }, { "English", "Inglese" },
            { "Português (Brasil)", "Portoghese (Brasile)" }, { "Spanish", "Spagnolo" }, { "French", "Francese" },
            { "German", "Tedesco" }, { "Italian", "Italiano" }, { "Ready to play", "Pronto per giocare" },
            { "Open logs", "Apri i registri" }, { "Send error", "Invia segnalazione" }, { "Stop game", "Ferma il gioco" },
            { "PLAY", "GIOCA" }, { "Resolution", "Risoluzione" }, { "Window mode", "Modalità finestra" },
            { "Internal resolution", "Risoluzione interna" }, { "Scaling", "Ridimensionamento" },
            { "Scaling filter", "Filtro di ridimensionamento" }, { "VSync", "Sincronizzazione verticale" },
            { "Frame limit", "Limite FPS" }, { "Camera aspect", "Proporzioni" },
            { "Field of view", "Campo visivo" }, { "Controls", "Comandi" },
            { "Keyboard and mouse", "Tastiera e mouse" }, { "Controller", "Controller" }
        };
        private static readonly Dictionary<string, string> portuguese = new Dictionary<string, string>
        {
            { "Display", "Tela" }, { "Graphics", "Gráficos" }, { "Input", "Controles" }, { "Game", "Jogo" },
            { "BLACK | PC Launcher", "BLACK | Launcher para PC" },
            { "BLACK  /  RECOMPILED FOR PC", "BLACK  /  RECOMPILADO PARA PC" },
            { "Development build", "Versão de desenvolvimento" },
            { "The game works with keyboard, mouse and controller.", "O jogo funciona com teclado, mouse e controle." },
            { "Output resolution", "Resolução da janela" }, { "Window mode", "Modo de janela" },
            { "Windowed", "Em janela" }, { "Borderless", "Sem bordas" }, { "Fullscreen", "Tela cheia" },
            { "Internal resolution", "Resolução interna" }, { "Scaling", "Escala" },
            { "Fit", "Ajustar" }, { "Stretch", "Esticar" }, { "Integer", "Inteira" },
            { "Scaling filter", "Filtro de escala" }, { "Smooth", "Suave" }, { "Sharp", "Nítido" },
            { "VSync", "Sincronização vertical" }, { "Frame limit", "Limite FPS" },
            { "Camera aspect", "Proporção da câmera" }, { "Original", "Original" }, { "Motion blur", "Desfoque" },
            { "Vertical field of view", "Campo de visão vertical" }, { "Keep BLACK's original field of view and aim zoom. Uncheck to choose a vertical angle in degrees.", "Mantém o campo de visão e o zoom de mira originais de BLACK. Desmarque para escolher um ângulo vertical em graus." },
            { "Vertical field of view in vertical degrees. Higher values show more above and below; the camera aspect sets how wide the view is.", "Campo de visão vertical em graus. Valores maiores mostram mais áreas acima e abaixo; a proporção da câmera define a largura da visão." },
            { "Controls how the picture fits a resized window or a movie. Keep aspect ratio adds bars; Stretch fills the window.", "Define como a imagem se ajusta à janela ou ao vídeo. Manter a proporção adiciona barras; Esticar preenche a janela." },
            { "The size of the game window when windowed. A saved nonstandard size is retained as a saved choice.", "O tamanho da janela do jogo no modo em janela. Tamanhos personalizados salvos continuam disponíveis." },
            { "How many lines the game renders; the width follows the camera aspect, and the picture is scaled to the window. Higher is sharper and slower. The game's own Video Settings page (Options) changes this too.", "Quantidade de linhas renderizadas; a largura segue a proporção da câmera e a imagem é ajustada à janela. Valores maiores deixam a imagem mais nítida e reduzem o desempenho. A página Configurações de vídeo do jogo também altera este valor." },
            { "Sizing and filtering are independent. Smooth blends neighboring pixels; Sharp keeps distinct pixel edges.", "O tamanho e o filtro são independentes. Suave mistura pixels vizinhos; Nítido preserva as bordas dos pixels." },
            { "Limits display updates. This does not change the game's simulation speed.", "Limita as atualizações da imagem. Isso não altera a velocidade da simulação do jogo." },
            { "Changes how wide the camera can see. Resolution controls picture detail; field of view controls its vertical angle.", "Altera a largura da visão da câmera. A resolução controla os detalhes; o campo de visão controla o ângulo vertical." },
            { "Anti-aliasing", "Serrilhado" }, { "Anisotropic", "Anisotrópico" },
            { "Depth of field", "Prof. de campo" }, { "Sharpening", "Nitidez" },
            { "Ambient occlusion", "Oclusão amb." }, { "AO quality", "Qualidade AO" },
            { "Off", "Desativado" }, { "Low", "Baixa" }, { "Medium", "Média" }, { "High", "Alta" }, { "Ultra", "Máxima" },
            { "FXAA", "FXAA" }, { "SSAA 4x", "SSAA 4x" }, { "SSAO Classic", "SSAO clássico" },
            { "Effects of the GPU renderer, applied to the picture before the HUD. The game's Video Settings page", "Efeitos do renderizador GPU aplicados à imagem antes da interface. A página Configurações de vídeo do jogo" },
            { "(Options > V) changes them while playing. Internal resolution is on the Display tab.", "(Opções > V) permite alterá-los durante o jogo. A resolução interna fica na aba Tela." },
            { "FXAA smooths jagged edges at almost no cost. SSAA 4x renders twice as wide and high and averages down: the cleanest and the heaviest. Needs the GPU renderer.", "FXAA suaviza bordas serrilhadas com custo mínimo. SSAA 4x renderiza com o dobro da largura e altura e reduz a imagem: é a opção mais nítida e mais pesada. Requer o renderizador GPU." },
            { "Keeps textures sharp on floors and walls seen at a slant. Costs almost nothing. Needs the GPU renderer.", "Mantém nítidas as texturas de pisos e paredes vistas de lado. Tem custo mínimo. Requer o renderizador GPU." },
            { "Blurs what is behind the point you are looking at; the weapon and the HUD stay sharp. Needs the GPU renderer.", "Desfoca o que está atrás do ponto observado; a arma e a interface permanecem nítidas. Requer o renderizador GPU." },
            { "Brings back fine detail that filtering and scaling soften. Needs the GPU renderer.", "Recupera detalhes finos suavizados pelo filtro e pela escala. Requer o renderizador GPU." },
            { "Ambient occlusion adds contact shading using scene depth. Choose SSAO Classic, HBAO, HBAO+ or GTAO. Needs the GPU renderer.", "A oclusão de ambiente adiciona sombras de contato usando a profundidade da cena. Escolha SSAO clássico, HBAO, HBAO+ ou GTAO. Requer o renderizador GPU." },
            { "Higher quality increases AO sampling density and radius, with more GPU work. All levels use scene resolution and depth-aware blur. Choose an AO method to enable the effect.", "Qualidades maiores aumentam a amostragem e o alcance da oclusão, exigindo mais da GPU. Todos os níveis usam a resolução da cena e desfoque sensível à profundidade. Escolha um método para ativar o efeito." },
            { "Controller only ignores the keyboard and mouse in the game; keyboard and mouse only ignores the pad.", "Somente controle ignora teclado e mouse no jogo; somente teclado e mouse ignora o controle." },
            { "Raw mouse input, no acceleration. 1.00 is 0.06 degrees per mouse count.", "Entrada direta do mouse, sem aceleração. 1,00 equivale a 0,06 grau por unidade de movimento." },
            { "How often the game's clock steps. Presented frames match it, so 240 needs a fast PC and a 240 Hz display to show.", "Frequência do relógio do jogo. Os quadros exibidos acompanham esse valor; 240 exige um PC rápido e uma tela de 240 Hz." },
            { "Draws the game on the graphics card, which holds 60 FPS at 3440x1440. Off uses the CPU renderer, which is far slower at high resolutions.", "Renderiza o jogo na placa de vídeo e mantém 60 FPS em 3440×1440. Desativado usa o renderizador da CPU, bem mais lento em altas resoluções." },
            { "Runs the game clock faster so every presented frame is a simulated frame. Off keeps the original 30 Hz clock.", "Acelera o relógio do jogo para que cada quadro exibido também seja simulado. Desativado mantém os 30 Hz originais." },
            { "Game menu language", "Idioma do jogo" }, { "Menu language", "Idioma" },
            { "Selects the game's text bank. The launcher installs this choice before each run; English is selected by default.", "Escolhe o idioma dos textos do jogo e do launcher. A opção é aplicada antes de cada partida; English é o padrão." },
            { "English", "English" }, { "Português (Brasil)", "Português (Brasil)" },
            { "Skip movies", "Pular vídeos" }, { "Advance startup menus automatically (first 65 seconds)", "Avançar menus iniciais automaticamente (primeiros 65 segundos)" },
            { "Detailed visual trace", "Rastreamento gráfico detalhado" }, { "Frames (start-end/step)", "Quadros (início-fim/intervalo)" },
            { "Detailed DSP state trace (GP/EP stack)", "Rastreamento detalhado do DSP (pilhas GP/EP)" },
            { "GPU draw log (frame,count)", "Registro de desenho GPU (quadro,quantidade)" },
            { "Records detailed [KTRACE] drawing data in reports\\...\\visual-trace.log (maximum 1 MB). Requires CPU Kelvin; turn off GPU renderer for this diagnostic. The renderer synchronizes work during collection, which can change frame timing. Use an untraced run for performance comparisons.", "Registra dados [KTRACE] detalhados em reports\\...\\visual-trace.log (máximo de 1 MB). Requer CPU Kelvin; desative o renderizador GPU para este diagnóstico. A coleta sincroniza o renderizador e pode alterar o tempo dos quadros. Compare desempenho em uma partida sem rastreamento." },
            { "Examples: 120, 120-180, or 120-240/30. Requires CPU Kelvin and can change frame timing; compare performance with a normal run.", "Exemplos: 120, 120-180 ou 120-240/30. Requer CPU Kelvin e pode alterar o tempo dos quadros; compare com uma partida normal." },
            { "Records GP/EP state, EP output FIFO activity, and final PCM levels about once per second in reports\\...\\audio-dsp-trace.log (maximum 1 MB). No audio is saved; short stack events may be missed.", "Registra o estado GP/EP, a atividade da fila de saída EP e os níveis PCM finais aproximadamente uma vez por segundo em reports\\...\\audio-dsp-trace.log (máximo de 1 MB). O áudio não é salvo; eventos rápidos podem passar despercebidos." },
            { "Leave blank to keep this diagnostic off. Example: 2400,2 records at most 2 frames to reports\\...\\kgpu-draw-trace.log (maximum 1 MB). Requires GPU Kelvin and adds overhead during those frames.", "Deixe vazio para desativar este diagnóstico. Exemplo: 2400,2 registra até 2 quadros em reports\\...\\kgpu-draw-trace.log (máximo de 1 MB). Requer GPU Kelvin e adiciona custo nesses quadros." },
            { "Renderer threads", "Threads do renderizador" }, { "Key bindings...", "Atalhos de teclado..." },
            { "Invert vertical mouse look", "Inverter movimento vertical do mouse" },
            { "Display mode", "Modo de exibição" },
            { "FPS limit", "Limite de FPS" }, { "Window size", "Resolução" },
            { "Ready to play", "Pronto para jogar" }, { "PC Release build is missing", "A versão Release do jogo não foi encontrada" },
            { "Game running", "Jogo em execução" }, { "Unable to launch", "Não foi possível iniciar" },
            { "Stopping game...", "Encerrando o jogo..." }, { "Game stopped", "Jogo encerrado" }, { "Game closed", "Jogo fechado" },
            { "Open logs", "Abrir registros" }, { "Controls", "Controles" }, { "Stop game", "Parar jogo" }, { "PLAY", "JOGAR" },
            { "Send error", "Enviar erro" }, { "Send error report", "Enviar relatório de erro" },
            { "Describe what happened", "Descreva o que aconteceu" },
            { "Attach the listed logs on GitHub before submitting. Use Attach files or drag them into the description, then review and submit.", "Anexe os registros listados no GitHub antes de enviar. Use Anexar arquivos ou arraste-os para a descrição; depois revise e publique." },
            { "The GitHub report is generated in English. Logs remain local; no files are attached automatically. Add any extra information under Additional notes on GitHub.", "O relatório do GitHub é gerado em inglês. Os registros permanecem locais; nenhum arquivo é anexado automaticamente. Adicione informações extras em Observações no GitHub." },
            { "Logs to attach from the latest session", "Registros para anexar da sessão mais recente" },
            { "A local Markdown draft is saved and copied. No files are uploaded by the launcher.", "Um rascunho Markdown será salvo e copiado. O launcher não envia arquivos." },
            { "Open logs folder", "Abrir pasta dos registros" }, { "Could not open logs folder", "Não foi possível abrir a pasta dos registros" },
            { "Attach this log:", "Anexe este registro:" }, { "Also attach if available:", "Anexe também, se disponível:" },
            { "Optional diagnostic log:", "Registro de diagnóstico opcional:" },
            { "No launcher session folder was found.", "Nenhuma pasta de sessão do launcher foi encontrada." },
            { "The main session log was not found.", "O registro principal da sessão não foi encontrado." },
            { "Detected information from the latest session", "Informações detectadas da última sessão" },
            { "A local Markdown draft is saved and copied. Logs stay on this PC; attach them manually if needed.", "Um rascunho Markdown será salvo e copiado. Os registros ficam neste PC; anexe-os manualmente se necessário." },
            { "Prepare report and open GitHub", "Preparar relatório e abrir o GitHub" },
            { "Describe what happened before preparing the report.", "Descreva o que aconteceu antes de preparar o relatório." },
            { "The report was saved and copied. Review the pre-filled issue on GitHub, then click Submit. If the browser text was shortened, the clipboard has the full report.", "O relatório foi salvo e copiado. Revise o relato já preenchido no GitHub e clique em enviar. Se o texto no navegador tiver sido reduzido, a área de transferência contém o relatório completo." },
            { "Report ready", "Relatório pronto" }, { "Could not prepare report: ", "Não foi possível preparar o relatório: " },
            { "# BLACK PC error report", "# Relatório de erro do BLACK PC" },
            { "## Description", "## Descrição" }, { "## Detected diagnostics", "## Diagnóstico detectado" },
            { "The launcher does not upload logs. Attach the relevant files through GitHub's issue form.", "O launcher não envia os registros. Anexe os arquivos relevantes pelo formulário de relato do GitHub." },
            { "No launcher session data was found.", "Nenhum dado de sessão do launcher foi encontrado." },
            { "Session folder: ", "Pasta da sessão: " }, { "Process exit code: ", "Código de saída do processo: " },
            { "Started (UTC): ", "Início (UTC): " }, { "Game menu language: ", "Idioma do jogo: " },
            { "Game executable SHA-256: ", "SHA-256 do executável: " }, { "Retail game SHA-256: ", "SHA-256 do jogo original: " },
            { "Classification: process exited with a nonzero code.", "Classificação: o processo terminou com código diferente de zero." },
            { "Classification: no nonzero exit code detected; the reported symptom needs the user's description.", "Classificação: nenhum código de erro foi detectado; descreva o sintoma observado." },
            { "No lines marked error/failure/exception were detected in session.log.", "Nenhuma linha com indicação de erro/falha/exceção foi encontrada em session.log." },
            { "Recent error-like lines from session.log:", "Linhas recentes com possível erro em session.log:" },
            { "Recent error-like lines from kernel.log:", "Linhas recentes com possível erro em kernel.log:" },
            { "No error-like lines were detected in kernel.log.", "Nenhuma linha com possível erro foi encontrada em kernel.log." },
            { "Diagnostic files in this session: ", "Arquivos de diagnóstico desta sessão: " },
            { "Session diagnostics could not be read: ", "Não foi possível ler o diagnóstico da sessão: " }, { "none", "nenhum" },
            { "Settings could not be saved: ", "Não foi possível salvar as configurações: " },
            { "Could not open logs", "Não foi possível abrir os registros" }, { "BLACK PC controls", "Controles do BLACK PC" },
            { "Close the existing game before Play. This launcher can stop sessions it starts.", "Feche o jogo que já está aberto antes de iniciar. Este launcher pode encerrar as partidas que ele inicia." },
            { "Keyboard and mouse are ready; a controller is optional. Startup may take a minute.\nSaves stay in the local save folder.", "Teclado e mouse estão prontos; o controle é opcional. A inicialização pode levar um minuto.\nOs jogos salvos ficam na pasta local." },
            { "Visual trace may change frame timing. Compare performance with a normal run.\nDetailed data: visual-trace.log (max 1 MB).", "O rastreamento gráfico pode alterar o tempo dos quadros. Compare o desempenho com uma partida normal.\nDados detalhados: visual-trace.log (máx. 1 MB)." },
            { "DSP trace samples GP/EP and final PCM levels about once per second; timing may shift slightly.\nDetailed data: audio-dsp-trace.log (max 1 MB).", "O rastreamento DSP registra GP/EP e níveis PCM finais aproximadamente uma vez por segundo; o tempo pode variar um pouco.\nDados detalhados: audio-dsp-trace.log (máx. 1 MB)." },
            { "GPU draw logging adds overhead on selected frames.\nDetailed data: kgpu-draw-trace.log (max 1 MB).", "O registro de desenho GPU adiciona custo nos quadros selecionados.\nDados detalhados: kgpu-draw-trace.log (máx. 1 MB)." },
            { "Use the BLACK PC window to play.\nStop ends the game process; finish saving first.", "Jogue pela janela BLACK PC.\nParar encerra o processo do jogo; salve antes." },
            { "Open logs for this session's output and launch settings.", "Abra os registros para ver a saída desta partida e as configurações de inicialização." },
            { "Session logging error: ", "Erro ao registrar a partida: " },
            { "Close the game window or use Stop game to end the session. Finish saving before closing.", "Feche a janela do jogo ou use Parar jogo para encerrar a partida. Salve antes de fechar." },
            { "Bind ", "Configurar: " }, { "Press a key or click a mouse button, or turn the wheel, for\n", "Pressione uma tecla, clique em um botão do mouse ou gire a roda para\n" },
            { "Clear this binding", "Limpar este atalho" }, { "Cancel", "Cancelar" },
            { "F11 toggles fullscreen and cannot be bound.", "F11 alterna a tela cheia e não pode ser atribuído." },
            { "That key cannot be bound.", "Essa tecla não pode ser atribuída." },
            { "Keyboard and mouse bindings", "Atalhos de teclado e mouse" },
            { "Double-click a slot (or select it and press Enter), then press the key or mouse button.", "Clique duas vezes em um campo (ou selecione e pressione Enter) e depois pressione a tecla ou o botão do mouse." },
            { "Action", "Ação" }, { "Binding 1", "Atalho 1" }, { "Binding 2", "Atalho 2" }, { "Binding 3", "Atalho 3" },
            { "Reset to defaults", "Restaurar padrões" }, { "OK", "OK" },
            { "Escape (pause / back) and F11 (fullscreen) are fixed. Enter always confirms; arrow keys and the wheel move through menus.", "Escape (pausar / voltar) e F11 (tela cheia) são fixos. Enter sempre confirma; as setas e a roda percorrem os menus." },
            { "Shared keys: ", "Teclas compartilhadas: " }, { ". Both actions will fire.", ". As duas ações serão executadas." },
            { "BLACK PC launcher", "Launcher BLACK PC" }, { "BLACK PC", "BLACK PC" },
            { "PC game already running (PID ", "O jogo já está em execução (PID " },
            { "Close the existing game before Play.", "Feche o jogo que já está aberto antes de iniciar." },
            { "BLACK PC  /  Local saves", "BLACK PC  /  Jogos salvos locais" },
            { "upscaled", "ampliada" }, { "saved", "salvo" }, { "Custom", "Personalizada" },
            { "Flat", "Original" }, { "Default", "Padrão" }, { "Auto", "Automático" },
            { "Keyboard and mouse", "Teclado e mouse" }, { "Controller", "Controle" },
            { "Mouse sensitivity", "Sensibilidade do mouse" }, { "Prompt device", "Dispositivo dos avisos" },
            { "Input mode", "Modo de entrada" }, { "Video settings", "Configurações de vídeo" },
            { "The game works with keyboard, mouse and controller.\nThis is an experimental build; crashes and inaccuracies remain.", "O jogo funciona com teclado, mouse e controle.\nEsta é uma versão experimental; ainda podem ocorrer falhas e imprecisões." },
            { "Window", "Janela" }, { "Filter", "Filtro" },
            { "Borderless fullscreen", "Tela cheia sem bordas" }, { "Keep aspect ratio", "Manter proporção" },
            { "Stretch to fill", "Esticar para preencher" }, { "Integer scaling", "Escala inteira" },
            { "Unlimited", "Sem limite" }, { "Standard (4:3)", "Padrão (4:3)" },
            { "Widescreen (16:9)", "Widescreen (16:9)" }, { "Ultrawide (21:9)", "Ultrawide (21:9)" },
            { "Ultrawide (32:9)", "Super Ultra-Wide (32:9)" }, { "Internal", "Interna" },
            { "720p (default)", "720p (padrão)" }, { "SSAA 4x (2 x 2)", "SSAA 4x (2 x 2)" },
            { "Devices", "Dispositivos" }, { "Prompts", "Botões" },
            { "Sensitivity", "Sensibilidade" }, { "0.06° / count", "0,06° / unidade" },
            { "Keyboard, mouse, pad", "Teclado / mouse / controle" },
            { "Keyboard and mouse only", "Somente teclado e mouse" }, { "Follow the device I use", "Dispositivo em uso" },
            { "Field of view", "FOV" },
            { "Presentation frame limit", "Limite de quadros exibidos" },
            { "Scaling mode", "Modo de escala" }, { "Timing rate", "Frequência do relógio" },
            { "Input devices", "Dispositivos de entrada" }, { "Button prompts", "Comandos dos botões" },
            { "Original field of view", "Campo de visão original" }, { "Vertical field of view in degrees", "Campo de visão vertical em graus" },
            { "Move forward", "Mover para frente" }, { "Move back", "Mover para trás" },
            { "Move left", "Mover para a esquerda" }, { "Move right", "Mover para a direita" },
            { "Look left (keys)", "Olhar para a esquerda (teclas)" }, { "Look right (keys)", "Olhar para a direita (teclas)" },
            { "Look up (keys)", "Olhar para cima (teclas)" }, { "Look down (keys)", "Olhar para baixo (teclas)" },
            { "unbound", "sem atribuição" }, { "Left Ctrl", "Ctrl esquerdo" }, { "Right Ctrl", "Ctrl direito" },
            { "Ctrl (either)", "Ctrl (qualquer lado)" }, { "Left Shift", "Shift esquerdo" }, { "Right Shift", "Shift direito" },
            { "Shift (either)", "Shift (qualquer lado)" }, { "Left Alt", "Alt esquerdo" }, { "Right Alt", "Alt direito" },
            { "Alt (either)", "Alt (qualquer lado)" }, { "Left mouse button", "Botão esquerdo do mouse" },
            { "Right mouse button", "Botão direito do mouse" }, { "Middle mouse button", "Botão do meio do mouse" },
            { "Mouse button 4", "Botão 4 do mouse" }, { "Mouse button 5", "Botão 5 do mouse" },
            { "Mouse wheel up", "Roda do mouse para cima" }, { "Mouse wheel down", "Roda do mouse para baixo" },
            { "Fire", "Atirar" }, { "Aim / zoom", "Mirar / zoom" }, { "Reload", "Recarregar" },
            { "Crouch", "Agachar" }, { "Use / pick up", "Usar / pegar" }, { "Switch weapon", "Trocar arma" },
            { "Switch weapon (other way)", "Trocar arma (sentido contrário)" }, { "Throw grenade", "Arremessar granada" },
            { "Fire mode", "Modo de disparo" }, { "Suppressor", "Silenciador" }, { "Melee attack", "Ataque corpo a corpo" },
            { "Use health pack", "Usar kit médico" }, { "Cycle item (White)", "Alternar item (Branco)" },
            { "Back / objectives", "Voltar / objetivos" }, { "Start (pause)", "Start (pausar)" },
            { "KEYBOARD AND MOUSE (no controller needed)", "TECLADO E MOUSE (sem controle)" },
            { "Look: move the mouse (raw input, no acceleration).", "Olhar: mova o mouse (entrada direta, sem aceleração)." },
            { "Menus: arrow keys or the mouse wheel to move, Enter / Space / left click to select,", "Menus: use as setas ou a roda do mouse para navegar; Enter, Espaço ou clique esquerdo seleciona." },
            { "Backspace / right click / Escape to go back. Enter also confirms \"to continue\" prompts.", "Backspace, clique direito ou Escape volta. Enter também confirma avisos de \"pressione para continuar\"." },
            { "Escape pauses a mission.", "Escape pausa a missão." }, { "CONTROLLER ONLY", "SOMENTE CONTROLE" },
            { "CONTROLLER (optional)", "CONTROLE (opcional)" }, { "Any XInput controller works as before.", "Qualquer controle XInput funciona como antes." },
            { "Keyboard and mouse gameplay and menu controls are disabled.", "Os comandos de teclado e mouse para jogo e menus ficam desativados." },
            { "Controller input is disabled.", "Os comandos do controle ficam desativados." },
            { "Prompts follow whichever device you used last.", "Os comandos na tela acompanham o último dispositivo usado." },
            { "Prompts show controller buttons.", "Os comandos na tela mostram os botões do controle." },
            { "Prompts show keyboard and mouse bindings.", "Os comandos na tela mostram os atalhos de teclado e mouse." },
            { "Leaving the game window pauses a mission.", "Sair da janela do jogo pausa a missão." },
            { "F11 toggles fullscreen. Movies: Escape skips the current one.", "F11 alterna a tela cheia. Vídeos: Escape pula o vídeo atual." },
            { "The size of the game window", "Tamanho da janela do jogo" },
            { "480p", "480p" }, { "720p", "720p" }, { "1080p", "1080p" }, { "1440p", "1440p" }, { "2160p", "2160p" },
            { "60 steps per second", "60 etapas por segundo" }, { "120 steps per second", "120 etapas por segundo" }, { "240 steps per second", "240 etapas por segundo" },
            { "Faster timing (the original game ran at 30)", "Ritmo acelerado (o jogo original rodava a 30)" },
            { "GPU renderer (Direct3D 11)", "Renderizador GPU (Direct3D 11)" },
            { "FPS", "FPS" },
            { "Original keeps BLACK's own motion blur. Off disables that effect while preserving other post effects.", "Original mantém o desfoque de movimento de BLACK. Desativado remove esse efeito e preserva os demais efeitos de imagem." },
            { "Key bindings", "Atalhos de teclado" }, { "Anisotropic filtering", "Filtragem anisotrópica" },
            { "Ambient occlusion method", "Método de oclusão de ambiente" }, { "Ambient occlusion quality", "Qualidade da oclusão de ambiente" },
            { "Camera", "Proporção" },
            { "Select a language", "Selecione um idioma" },
            { "PC game already running", "O jogo já está em execução" },
            { "Game status", "Estado do jogo" },
            { "WASD moves, the mouse looks, the left button fires.", "WASD move; mouse olha; botão esquerdo atira." },
            { "Escape pauses a mission (and goes back in menus). Leaving the window pauses too.", "Escape pausa a missão e volta nos menus. Sair da janela também pausa." },
            { "F11 toggles fullscreen. In-game prompts show your real bindings.", "F11 alterna tela cheia. Os comandos na tela mostram seus atalhos." },
            { " default on this ", " padrão neste PC com " }, { "-thread PC", " threads" }
        };

        internal static string Current { get { return current; } }

        internal static void Set(string language)
        {
            current = GameLanguageBanks.Normalize(language);
        }

        internal static string Text(string english)
        {
            if (english == null || current == GameLanguageBanks.English) return english;
            Dictionary<string, string> translations;
            Regex matcher;
            lock (languageCacheLock)
            {
                if (!translationsByLanguage.TryGetValue(current, out translations))
                {
                    Dictionary<string, string> catalog = current == GameLanguageBanks.PortugueseBrazil ? portuguese :
                        current == GameLanguageBanks.Spanish ? spanish : current == GameLanguageBanks.French ? french :
                        current == GameLanguageBanks.German ? german : current == GameLanguageBanks.Italian ? italian : null;
                    Dictionary<string, string> generated;
                    GeneratedCatalogs.TryGetValue(current, out generated);
                    if ((generated == null || generated.Count == 0) && (catalog == null || catalog.Count == 0)) return english;

                    translations = new Dictionary<string, string>(System.StringComparer.Ordinal);
                    if (catalog != null)
                        foreach (KeyValuePair<string, string> entry in catalog) translations[entry.Key] = entry.Value;
                    if (generated != null)
                        foreach (KeyValuePair<string, string> entry in generated) translations[entry.Key] = entry.Value;
                    translationsByLanguage[current] = translations;
                    string[] keys = translations.Keys.Where(key => !System.String.IsNullOrEmpty(key))
                        .OrderByDescending(key => key.Length).ToArray();
                    if (keys.Length == 0) return english;
                    string[] patterns = keys.Select(key =>
                        (System.Char.IsLetterOrDigit(key[0]) || key[0] == '_' ? @"(?<![\p{L}\p{N}_])" : "") +
                        Regex.Escape(key) +
                        (System.Char.IsLetterOrDigit(key[key.Length - 1]) || key[key.Length - 1] == '_' ? @"(?![\p{L}\p{N}_])" : "")
                    ).ToArray();
                    matcher = new Regex(System.String.Join("|", patterns), RegexOptions.CultureInvariant);
                    matchersByLanguage[current] = matcher;
                }
                else if (!matchersByLanguage.TryGetValue(current, out matcher)) return english;
            }
            string translated;
            if (translations.TryGetValue(english, out translated)) return translated;
            return matcher.Replace(english, match => translations[match.Value]);
        }

        internal static void Apply(Control parent)
        {
            if (!(parent is ComboBox) && !(parent is TextBoxBase) && !(parent is UpDownBase))
            {
                if (parent.Tag == null) parent.Tag = parent.Text;
                string english = parent.Tag as string;
                if (!System.String.IsNullOrEmpty(english)) parent.Text = Text(english);
            }
            parent.Refresh();
            foreach (Control child in parent.Controls) Apply(child);
        }
    }
}
