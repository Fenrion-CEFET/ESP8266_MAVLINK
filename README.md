ESP8266_MAVLINK (Equipe FENRION)

Ponte de telemetria sem fio entre a Pixhawk 2.4.8 e o notebook da estação de solo. A ESP8266 recebe as mensagens MAVLink pela porta serial da Pixhawk (TELEM2) e repassa pelo Wi-Fi. Assim o Mission Planner, o QGroundControl ou um script em Python conversam com o drone sem cabo e sem rádio de telemetria.

Pixhawk (TELEM2)  --fios-->  ESP8266  --Wi-Fi-->  Notebook (Mission Planner / Python)

Este guia foi escrito para quem acabou de instalar a Arduino IDE 2.x e nunca gravou uma ESP8266.

0. Antes de começar: valores que dependem do código

Alguns números precisam ser iguais no código, na Pixhawk e no notebook. Abra o arquivo .ino e anote os valores abaixo antes de seguir.

O que procurar no código	Exemplo de linha	Onde esse valor é usado
Velocidade da serial	Serial.begin(57600);	Parâmetro SERIAL2_BAUD da Pixhawk (seção 5)
Modo da rede Wi-Fi	WiFi.softAP(...) ou WiFi.begin(...)	Como o notebook se conecta (seção 6)
Nome e senha da rede	const char* ssid = "...";	Seção 4
Protocolo e porta	udp.begin(14550); ou WiFiServer server(5760);	Conexão no Mission Planner ou Python (seção 6)
WiFi.softAP significa que a ESP8266 cria a própria rede (o notebook entra na rede dela). O endereço da ESP costuma ser 192.168.4.1.
WiFi.begin significa que a ESP8266 entra numa rede existente (roteador ou celular). O endereço aparece no Serial Monitor.

1. Preparar a Arduino IDE (só na primeira vez)
1.1 Instalar o pacote das placas ESP8266
Abra a Arduino IDE.
Vá em File > Preferences.
No campo Additional boards manager URLs, cole o endereço abaixo. Se já houver outro endereço (por exemplo o da ESP32), clique no ícone ao lado do campo e coloque este numa linha nova.
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
Clique em OK.
Abra o Boards Manager (ícone de placas no menu lateral esquerdo, ou Tools > Board > Boards Manager).
Pesquise esp8266 e instale esp8266 by ESP8266 Community.

1.2 Instalar bibliotecas (se o código pedir)

Olhe as linhas #include no início do código:

ESP8266WiFi.h, WiFiUdp.h, ESP8266WebServer.h: já vêm com o pacote da ESP8266. Não precisa instalar nada.
mavlink.h ou MAVLink.h (ou outra que não seja da ESP8266): abra o Library Manager (ícone de livros no menu lateral), pesquise o nome e instale.

Se ao compilar aparecer No such file or directory com o nome de um arquivo .h, é uma biblioteca faltando: pesquise esse nome no Library Manager.

1.3 Baixar o código deste repositório
Nesta página do GitHub, clique no botão verde Code e depois em Download ZIP.
Descompacte o arquivo.
A Arduino IDE exige que o arquivo .ino fique dentro de uma pasta com o mesmo nome do arquivo (exemplo: esp8266_mavlink/esp8266_mavlink.ino). Se a IDE perguntar se pode criar a pasta ao abrir, clique em OK.
Abra o arquivo .ino na IDE (File > Open).
2. Ligar a ESP8266 no computador
Ligue a placa (NodeMCU ou Wemos D1 mini) no computador com um cabo micro-USB de dados. Alguns cabos só carregam e não funcionam.
Se o Windows não reconhecer a placa, instale o driver do chip USB dela. Olhe o chip pequeno perto do conector USB:
escrito CH340: pesquise "driver CH340" (fabricante WCH)
escrito CP2102: pesquise "driver CP210x" (fabricante Silicon Labs)

Importante: a ESP8266 usa os mesmos pinos para gravar pelo USB e para conversar com a Pixhawk. Desligue os fios da Pixhawk (TX e RX) sempre que for gravar.

3. Configurar a placa na IDE

Em Tools, ajuste:

Opção	Valor
Board	NodeMCU 1.0 (ESP-12E Module) para NodeMCU, ou LOLIN(WEMOS) D1 R2 & mini para Wemos D1 mini (em esp8266)
Port	a porta COM que apareceu ao ligar a placa
Upload Speed	115200 (se der erro de gravação, mantenha este valor)

Para descobrir qual é a porta COM: veja a lista em Tools > Port com a placa desligada, ligue a placa e veja qual porta apareceu.

4. Colocar a rede Wi-Fi e gravar
No código, procure as linhas de nome e senha da rede (ssid e password).
Se o código usa WiFi.softAP: escolha um nome e uma senha para a rede que a ESP vai criar (a senha precisa ter pelo menos 8 caracteres).
Se o código usa WiFi.begin: coloque o nome e a senha da rede existente. A ESP8266 só funciona em redes 2,4 GHz.
Nunca envie o código para o GitHub com a senha preenchida. Volte para "SUA_REDE" e "SUA_SENHA" antes de fazer upload.
Confira que os fios da Pixhawk estão desligados da ESP.
Clique em Upload (seta para a direita).
Quando terminar (Hard resetting via RTS pin...), abra o Serial Monitor (ícone de lupa no canto superior direito), ajuste a velocidade para o mesmo valor do Serial.begin do código e aperte o botão RST da placa. Se o código imprimir mensagens, deve aparecer o endereço IP.
5. Ligar na Pixhawk e configurar os parâmetros
5.1 Fios

Use a porta TELEM2 da Pixhawk 2.4.8 (conector de 6 pinos). Contando do pino 1 (o do fio vermelho do cabo original):

Pino TELEM2	Sinal	Liga na ESP8266
1	+5 V	VIN (NodeMCU) ou 5V (Wemos). Nunca no pino 3V3
2	TX (saída da Pixhawk)	RX
3	RX (entrada da Pixhawk)	TX
4	CTS	não liga
5	RTS	não liga
6	GND	GND

O TX de um lado sempre vai no RX do outro. Se a telemetria não funcionar, a primeira coisa a testar é inverter TX e RX.

Se a ESP reiniciar sozinha quando os motores giram ou o Wi-Fi ficar instável, alimente a ESP por um regulador de 5 V separado (BEC) e mantenha só TX, RX e GND ligados na Pixhawk.

5.2 Parâmetros da Pixhawk

Ligue a Pixhawk no Mission Planner pelo cabo USB, vá em CONFIG > Full Parameter List, ajuste e clique em Write Params:

Parâmetro	Valor	Significado
SERIAL2_PROTOCOL	2	MAVLink 2 na TELEM2
SERIAL2_BAUD	57 para 57600, 115 para 115200, 921 para 921600	tem que ser igual ao Serial.begin do código
BRD_SER2_RTSCTS	0	desliga o controle de fluxo (não estamos usando CTS e RTS)

Depois de gravar os parâmetros, reinicie a Pixhawk (desligue e ligue).

6. Conectar o notebook
Ligue a Pixhawk com a ESP conectada.
Conecte o notebook na rede Wi-Fi:
modo softAP: entre na rede que a ESP criou (o nome que você colocou no código)
modo WiFi.begin: o notebook precisa estar na mesma rede que a ESP
6.1 Mission Planner
No canto superior direito, troque a caixa de conexão de COM para UDP (se o código usa udp) ou TCP (se o código usa WiFiServer).
Clique em CONNECT.
UDP: confirme a porta do código (normalmente 14550).
TCP: informe o IP da ESP (ex.: 192.168.4.1) e a porta do código (ex.: 5760).
Se der certo, os parâmetros começam a carregar e o horizonte artificial se mexe quando você inclina o drone.

Se o Windows perguntar sobre o firewall, clique em Permitir acesso.


6.2 Python (pymavlink)

Instale uma vez no terminal: pip install pymavlink

python
from pymavlink import mavutil

# UDP (troque a porta se o código usar outra):
con = mavutil.mavlink_connection("udpin:0.0.0.0:14550")
# TCP (descomente e ajuste IP e porta):
# con = mavutil.mavlink_connection("tcp:192.168.4.1:5760")

con.wait_heartbeat()
print("Conectado ao sistema", con.target_system)

while True:
    msg = con.recv_match(type="ATTITUDE", blocking=True)
    print(f"roll={msg.roll:.2f}  pitch={msg.pitch:.2f}  yaw={msg.yaw:.2f}")

Se aparecer Conectado ao sistema 1 e os ângulos mudarem quando você inclina o drone, o MAVLink está funcionando.

Atenção: só um programa por vez consegue ouvir a mesma porta UDP. Feche o Mission Planner antes de rodar o script, ou vice-versa.

7. Problemas comuns
Sintoma	O que fazer
Porta COM não aparece	Trocar o cabo USB (precisa ser de dados); instalar driver CH340 ou CP210x
Erro ao gravar (Failed to connect ou timed out)	Desligar os fios da Pixhawk da ESP; tentar de novo segurando o botão FLASH da placa no início da gravação
Notebook não vê a rede da ESP	Conferir se a senha tem 8 caracteres ou mais; apertar RST na ESP
Conecta no Wi-Fi mas o Mission Planner não recebe nada	Inverter TX e RX; conferir se SERIAL2_BAUD é igual ao Serial.begin; conferir SERIAL2_PROTOCOL = 2; reiniciar a Pixhawk
Mission Planner conecta e cai	Wi-Fi fraco ou baud muito alto: testar 57600; aproximar o notebook
ESP reinicia sozinha	Alimentação fraca: usar BEC de 5 V separado
Script Python trava em wait_heartbeat	Fechar o Mission Planner; conferir protocolo (UDP ou TCP) e porta
Regras da equipe para este repositório
Nunca enviar senha de Wi-Fi no código.
Toda alteração enviada precisa ter uma descrição clara no commit (o que mudou e por quê).
Configurações testadas e aprovadas em voo (baud, porta, modo da rede) devem ser anotadas aqui no README.
