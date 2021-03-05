/*
 * main.c
 *  Um programa de controlo de persiana com comunica��o por porta
 *  s�rie.
 *
 *  Objetivo:
 *  Controlar uma persiana usando dois bot�es (cima e baixo), se
 *  se carregar rapidamente num dos bot�es a persiana abre/fecha
 *  completamente (modo autom�tico). Se se carregar lentamente num
 *  dos bot�es a persiana abre/fecha at� se deixar de carregar no
 *  respetivo bot�o (modo manual). Alternativamente, por
 *  computador (ou qualquer dispositivo capaz de enviar tramas por
 *  porta s�rie para o Arduino com c�digo ASCII) poder controlar o
 *  n�vel de abertura da persiana (fechada, aberta, apenas separar
 *  t�buas ou abrir at� uma determinada percentagem).
 *
 *  Pormenores contextuais:
 *   -No tempo em que ainda n�o se decidiu entre um clique
 *  "r�pido" e um clique "lento" a persiana j� se encontra em
 *  movimento.
 *   -Estando a persiana a abrir num sentido, em modo autom�tico,
 *  se se carregar no bot�o de sentido oposto, a persiana obedece
 *  mudando de dire��o.
 *   -Estando a persiana a abrir num sentido, em modo autom�tico,
 *  se se carregar no bot�o do mesmo sentido, a persiana para.
 *   -Estando a persiana a abrir num sentido, em modo manual, se
 *  se carregar no bot�o de sentido oposto, a persiana para e
 *  aguarda at� que um novo bot�o seja novamente carregado.
 *   -Os comandos por porta s�rie t�m prioridade absoluta em
 *  rela��o aos bot�es (comandos nunca ser�o ignorados nem
 *  adiados). E mesmo estando numa opera��o causada por comando
 *  de porta s�rie, um novo comando ser� sempre imediatamente
 *  respondido.
 *   -Considerou-se que o tempo que a persiana demora a abrir
 *  � de 13,2s (cronometrado) e que a sua velocidade de subida
 *  permanece constante, o que n�o � verdade e gera um erro
 *  cumulativo.
 *   -Considerou-se que a persiana consegue mudar de dire��o
 *  enquanto o motor est� ligado, sem qualquer problema e sem
 *  atrasos. Infelizmente, como a mudan�a de dire��o implica uma
 *  mudan�a brusca da fase a que a persiana est� ligada, esta
 *  troca pode, eventualmente, estragar a persiana. Deve-se,
 *  assim, evitar estas transi��es perigosas.
 *
 *  Estrutura do c�digo:
 *   Este c�digo, tal como a grande maioria de c�digos
 *   com o intuido de automatizar, � constitu�do de uma sec��o de
 *   inicializa��o que estabelece uma base para que o programa
 *   possa funcionar (estabelecer entradas/sa�das, configurar
 *   timers e comunica��o por porta s�rie) seguido de um ciclo que
 *   corre infinitamente que cont�m a aplica��o propriamente dita.
 *   Este ciclo segue uma l�gica de m�quina de estados, usando uma
 *   vari�vel que cont�m um n�mero simb�lico indicativo do estado
 *   atual, que por sua vez representa em que situa��o se encontra
 *   a persiana. Qualquer comando por porta s�rie ir� for�ar o
 *   estado a "obedecer" ao comando, independentemente do estado
 *   atual (com exce��o do estado de inicializa��o), sendo assim
 *   que se estabelece a prioridade a estes comandos. As sa�das
 *   (motor e sua dire��o) s�o controladas no in�cio de cada estado
 *   com o fim de evitar uma estrutura "switch" separada para o
 *   efeito.
 *
 *  Timer:
 *   Havia alguma liberdade com a escolha da base de tempo para o
 *   timer, no entanto, e prevendo  que controlar a persiana
 *   atrav�s do seu tempo de subida iria gerar um erro
 *   significativo, escolheu-se uma base de tempo de 1ms para
 *   atenuar este erro.
 *   Com uma base de 1ms a solu��o de maior prescaler, TP, e
 *   prescaler de "Clock" a 1 (para evitar afetar outros timers e
 *   processos), � com uma contagem de 125 (obtido por Excel):
 *    CP * TP * CNT = Fcpu * Tint
 *    1  * 128* 125 = 16M  * 1m
 *   N�o havendo outras exig�ncias provenientes do timer, ser�
 *   poupado o timer 1, e implementado o timer 2.
 *   Ser� implementado o modo normal cujo contador aumenta at�
 *   ocorrer overflow, gerando um pedido de interrup��o e
 *   reinicializando a 0. Em cada interrup��o o contador ser�
 *   colocado a 125 contagens do seu valor de overflow, de forma
 *   a perservar as 125 contagens necess�rias a interrup��es
 *   peri�dicas de 1 ms. Isto � feito atribuindo diretamente �
 *   vari�vel do contador o valor referido.
 *   A rotina de interrup��o do timer ir� decrementar a vari�vel
 *   "check_delay" at� esta atingir 0 (age como temporizador) e
 *   ir� aumentar ou reduzir a vari�vel "height" conforme o motor
 *   esteja a subir ou a descer (se o motor estiver desligado a
 *   vari�vel permanece inalterada).
 *
 *  Comunica��o s�rie:
 *   Utilizando um m�todo de comunica��o ass�ncrona, torna-se
 *   necess�rio definir uma Baudrate, que foi arbitrada como
 *   sendo de 57600. Vai-se utilizar o modo normal de amostragem,
 *   estabelecendo 16 amostras por cada bit recebido. Sendo a
 *   frequ�ncia do cristal associado ao ATMega328p de 16MHz, isto
 *   traduz-se num registo de UBRRO (seguindo f�rmula da
 *   datasheet):
 *    UBBRO = 16M/(16*57600)-1 = 16.361 (arredondando) = 16
 *    BAUDRATE = 16M/(16*(16-1)) = 58823,52941 bps
 *   Ou seja, um erro de 2,124% que n�o ser� muito significativo
 *   tendo em conta que n�o ser�o usados bits de paridade, e
 *   apenas 1 stop bit com 8 bits de dados (9bits no total) que
 *   corresponde a um erro m�ximo admiss�vel de cerca de 5,56%.
 *   S�o ativos os registos de leitura e escrita bem como de
 *   interrup��o por leitura. Sempre que � lido um valor � gerado
 *   o respetivo pedido de interrup��o que simplesmente guarda o
 *   valor recebido numa vari�vel e devolve-o (a devolu��o do valor
 *   apenas serve para quest�es informativas ao utilizador). Esta
 *   vari�vel ser� avaliada no main() de forma a averiguar se � um
 *   input v�lido e qual o comando a que lhe corresponde, de entre
 *   os seguintes:
 *    'u'-abrir completamente
 *    'g'-colocar as t�buas separadas sem abrir a persiana
 *    '0'~'9'- abrir at� x% (0% corresponde a fechar a persiana)
 *
 *  Debug:
 *   O debug n�o � da nossa autoria tendo sido utilizado para o
 *   efeito a biblioteca "serial.c" da autoria do docente Jo�o
 *   Paulo Sousa. Esta biblioteca redireciona a stream stdout
 *   colocando a componente ".put" de FDEV_SETUP_STREAM para a nova
 *   fun��o "usart_putchar()" que escreve, sempre que poss�vel, um
 *   caractere da string indicada por "printf()". A flag de
 *   FDEV_SETUP_STREAM � colocada em modo de escrita
 *   ("_FDEV_SETUP_WRITE"). (".get" n�o ter� nenhum valor pois
 *   apenas se pretende escrever para o PC)
 *   A fun��o "printf_init()" limita-se a atualizar o valor da
 *   vari�vel stdout para a nova configura��o descrita.
 *   Para ativar o modo Debug basta definir a constante DEBUG na
 *   linha de c�digo 143.
 *   O c�digo debug leva muito tempo a correr em rela��o ao resto
 *   do programa, e � apenas usado aquando do teste da persiana
 *   na fase de desenvolvimento.
 *
 *  Cabe�alho creado em: 26/11/2018 (p�s avalia��o presencial)
 *  C�digo creado em: 15/11/2018
 *      Autores: Carlos Manuel Santos Pinto
 *               Maria Sara Delgadinho Noronha
 */

#include <avr/interrupt.h>
#include "serial.h"

// DEBUG mode
//#define DEBUG

// Nomes simbolicos para os estados
#define INIT 0 // Inicializa��o
#define IDLE 1 // Aguarda Comandos
#define CLOSE_CHECK 2 // Fecha e decide entre manual e autom�tico
#define OPEN_CHECK 3 // Abre e decide entre manual e autom�tico
#define OPEN_AUTO 4 // Abre automaticamente
#define CLOSE_AUTO 5 // Fecha automaticamente
#define CLOSE_MANUAL 6 // Fecha manualmente
#define OPEN_MANUAL 7 // Abre manualmente
#define OPEN_X 8 // Abre/fecha at� X% da altura m�xima (altura de refer�ncia)
#define ILLEGAL 255 // Para estados imprevistos

#define T2BOTTOM 255-125 // Valor inicial de contagem de timer 2 (ver fun��o "config_timer2()")

#define MOTOR PB0 // Posi��o respetiva ao pino do motor (ativo a 0)
#define DIR PB1 // Posi��o respetiva ao pino da dire��o do motor (0 vai para cima, 1 vai para baixo)
#define CLOSE PD6 // Posi��o respetiva ao pino do bot�o de fecho (ativo a 0)
#define OPEN PD7 // Posi��o respetiva ao pino do bot�o de abertura (ativo a 0)

#define CHECK_TIME 500 // 0.5s para distinguir entre clique r�pido e lento
#define INIT_TIME 14000 // 14s para fechar totalmente (garantidamente)

#define MAX_HEIGHT 13200 //13.2s para chegar � m�xima altura (cronometrado - sujeito a erro)
#define OPEN_TIME 2500 // Corresponde ao tempo que a persiana demora a come�ar a abrir (t�buas deixam de tocar na base, tamb�m foi cronometrado)
#define OPEN_10 (MAX_HEIGHT-OPEN_TIME)/10 // Valor relativo (10%) de abertura descontando o tempo de abertura definido na linha anterior


#ifndef F_CPU
#define F_CPU 16000000ul // 16MHz de frequ�ncia de rel�gio do processador (para estabelecer a Baudrate)
#endif

#define BAUD 57600ul // Baudrate de 57600 simbolos/s
#define UBBR_VAL ((F_CPU/(BAUD*16))-1) // 16 amostras por s�mbolo (modo normal)

uint8_t OpenBtn = 0; // Vari�vel auxiliar de verifica��o (Bot�o de abertura pressionado -> 1)
uint8_t CloseBtn = 0; // Vari�vel auxiliar de verifica��o (Bot�o de fecho pressionado -> 1)
uint8_t RE_OpenBtn = 0; // Rising Edge de OpenBtn
uint8_t RE_CloseBtn = 0; // Rising Edge de CloseBtn
volatile uint8_t USB_input = 0; // Vari�vel auxiliar que vai guardar o caracter recebido por porta s�rie
uint8_t state = INIT; // Estado atual
unsigned int height_reference = 0; // Altura de refer�ncia (apenas pertinente no estado OPEN_X)
volatile unsigned int height = MAX_HEIGHT; // Altura atual, inicia no topo
volatile unsigned int check_delay = INIT_TIME; // Tempo de espera na decis�o entre clique r�pido e lento. Tamb�m �
                                               // usado como timer na inicializa��o para garantir que a persiana fecha
#ifdef DEBUG
uint8_t printfstate = 254; // �ltimo estado impresso por printf (apenas pertinente no caso de debug)
#endif

/* Configura pinos de entrada/sa�da */
void config_io (void){
  DDRB |= ((1<<MOTOR) | (1<<DIR)); // configura os pinos respetivos ao motor e sua dire��o como sa�das
  DDRD &= (~(1<<CLOSE) & ~(1<<OPEN)); // configura os pinos respetivos aos bot�es de abertura/fecho como entradas

  PORTB |= (1<<MOTOR); // Garante que o motor est�, inicialmente, desligado
}

/* Configura timer 2 para contar em ciclos de 1ms:
 * Frequ�ncia do CPU = 16MHz;
 * TP = 128; CP = 1; CNT = 125 impulsos (255-130);
 * onde 130 � o valor inicial de contagem para que
 * sejam contados 125 impulsos at� overflow.
 * Tempo entre interrup��es � dado por:
 *   128*125/16M = 1ms */
void config_timer2 (void){
  TCCR2B = 0; // Para o timer
  TIFR2 |= (7<<TOV2); // Desliga quaisquer flags que estejam ativas
  TCCR2A = 0; // Modo de contagem normal
  TCNT2 = T2BOTTOM; // Posiciona o inicio da contagem no valor estabelecido em T2BOTTOM
  TIMSK2 = (1<<TOIE1); // Permite interrup��o por overflow
  TCCR2B = 5; // Inicia o timer com um prescaler : TP=128
}

void init_usart(){
	// Configura��o do conjunto de bits para determinar frequ�ncia de transi��o entre bits
	UBRR0 = UBBR_VAL;
	/* Significa que cada bit ser� amostrado 16 vezes
	 * Que por sua vez significa que teremos uma
	 * Baudrate efetiva de 58823.52941 Bps */

	UCSR0C = (3<<UCSZ00) // Cada segmento ter� 8 bits de informa��o �til (data bits),
		   | (0<<UPM00)  // sem paridade,
		   | (0<<USBS0); // e 1 bit de paragem

	UCSR0B = (1<<TXEN0) | (1<<RXEN0) | (1<<RXCIE0); // Permite leitura, escrita e interrup��o ap�s leitura
}

ISR (USART_RX_vect) { // Sempre que recebe dados por porta s�rie
 USB_input = UDR0; // ATMega recebe os dados do PC e guarda-os
 UDR0 = USB_input; // Envia os dados que recebeu, de volta para o PC
}

ISR (TIMER2_OVF_vect){ // Interrup��o gerada a cada 1ms
  TCNT2 = T2BOTTOM; // atualiza valor inicial de contagem do timer 2

  if ( !(PINB & (1<<MOTOR)) && !(PINB & (1<<DIR)) && (height)){ // se o motor estiver a fechar
    height--; // decrementa altura
  }
  else if ( !(PINB & (1<<MOTOR)) && (PINB & (1<<DIR)) && (height < MAX_HEIGHT)){ // se o motor estiver a abrir
    height++; // incrementa altura
  }

  if (check_delay){ // se check_delay ainda n�o atingiu 0
    check_delay--; // decrementa
  }
}



int main(){

  init_usart(); // Configura a comunica��o por porta s�rie
  config_io(); // Configura pinos de entrada e sa�da
  config_timer2(); // Configura timer 2
  sei(); // Ativar bit geral de interrup��es, permitindo interrup��es em geral

  #ifdef DEBUG
    printf_init();
    printf("\n____________________|DEBUG ON|____________________\n");
  #endif


  while(1){ // Ciclo infinito (Loop)

    // Leitura de entradas no mesmo instante
    RE_CloseBtn = (!(PIND & (1<<CLOSE))) && (!CloseBtn); // Ativo no flanco ascendente do botao de abertura
    CloseBtn = !(PIND & (1<<CLOSE)); // Botao de fecho (ativo a 1)
    RE_OpenBtn = (!(PIND & (1<<OPEN))) && (!OpenBtn); // Ativo no flanco ascentende do botao de abertura
    OpenBtn = !(PIND & (1<<OPEN)); // Botao de abertura (ativo a 1)

    /* A porta s�rie tem prioridade sobre os but�es, portanto assim que algo � lido, �
     * processado o que foi recebido e a m�quina de estados � for�ada ao estado adequado */
    if(USB_input!=0){ // Se algo foi lido por porta s�rie for�a a m�quina de estados a responder de acordo
      if (INIT!=state){
        if ('u' == USB_input){ // Se se premiu "u", abre completamente
          state = OPEN_AUTO; // Abre (completamente) em modo autom�tico
        }
        else if ('0' == USB_input) { // Se se premiu "0", fecha completamente
          state = CLOSE_AUTO; // Fecha (completamente) em modo autom�tico
        }
        else if (USB_input>'0' && USB_input<='9'){ // Foi premido um n�mero que n�o zero
          height_reference = OPEN_10*(USB_input-48)+OPEN_TIME; // Toma valores desde 10% a 90% de abertura, dependendo da tecla premida
          state = OPEN_X; // Abre/fecha at� height_reference
        }
        else if (USB_input == 'g'){ // Se se premiu "g", separa as t�buas sem abrir a persiana
          height_reference = OPEN_TIME; // Altura de abertura efetiva da persiana
          state = OPEN_X; // Abre/fecha at� ficar com as t�buas separadas
        }
      }

      USB_input = 0; // Reset da vari�vel de leitura
    }

    switch (state){
      case INIT: // 0 - Inicializa��o (fecha totalmente a persiana e ignora comandos do utilizador)
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB |= (1<<DIR); // com dire��o para cima (abre)

        if (!check_delay){ // Se j� passou tempo de inicializa��o (check_delay � usado como timer de inicializa��o apenas nesta linha)
          state = IDLE; // para de fechar e aguarda comandos do utilizador
        }break;

      case IDLE: // 1 - Espera por qualquer a��o (Inativo)
        PORTB |= (1<<MOTOR); // desliga motor

        if (RE_CloseBtn && height ){ // Quer-se fechar e ainda n�o est� fechado nem se est� a carregar no bot�o de abrir
          state = CLOSE_CHECK; // Fecha persiana e...
          check_delay = CHECK_TIME; //...inicializa contagem do tempo que se mant�m o bot�o carregado
        }
        else if (RE_OpenBtn && (MAX_HEIGHT != height) ){ // Quer-se abrir e ainda n�o est� aberto
          state = OPEN_CHECK; // Abre persiana e...
          check_delay = CHECK_TIME; // ...inicializa contagem do tempo que se mant�m o bot�o carregado
        }break;


      case CLOSE_CHECK: // 2 - Fecha e verifica quanto tempo se prime o bot�o
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB &= ~(1<<DIR); // com dire��o para baixo (fecha)

    	if (!height){ // Se j� fechou completamente
    	  state = IDLE; // para de fechar
    	}
        else if (!CloseBtn){ // Se j� n�o se est� a carrergar no bot�o (implica que foi um clique r�pido)
          state = CLOSE_AUTO; // fecha em modo autom�tico
        }
        else if (!check_delay){ // Se j� passou o tempo (implica que foi um clique lento)
          state = CLOSE_MANUAL; // fecha em modo manual
        }
        else if (OpenBtn) { // Se se carregar no bot�o de abertura
          state = IDLE; // Para de fechar
        }break;


      case OPEN_CHECK: // 3 - Abre e verifica quanto tempo se prime o bot�o
        PORTB &= (~(1<<MOTOR)); // Liga motor
        PORTB |= (1<<DIR); // com dire��o para cima (abre)

        if (MAX_HEIGHT == height){ // Se j� abriu completamente
          state = IDLE; // para de abrir
        }
        else if (!OpenBtn){ // Se j� n�o se est� a carrergar no bot�o (implica que foi um clique r�pido)
          state = OPEN_AUTO; // abre em modo autom�tico
        }
        else if (!check_delay){ // Se j� passou o tempo (implica que foi um clique lento)
          state = OPEN_MANUAL; // abre em modo manual
        }
        else if (CloseBtn){ // Se se carregar no bot�o de fecho
          state = IDLE; // Para de abrir
        }break;


      case OPEN_AUTO: // 4 - Abre at� a persiana ficar completamente aberta
        PORTB &= (~(1<<MOTOR)); // Liga motor
        PORTB |= (1<<DIR); // com dire��o para cima (abre)

        if (MAX_HEIGHT == height || OpenBtn){ // Se j� abriu completamente ou voltou-se a carregar no bot�o de abrir
          state = IDLE; // para de abrir
        }
        else if(CloseBtn){ // Se se carregou no bot�o de fecho
          state = CLOSE_CHECK; // Fecha persiana e...
          check_delay = CHECK_TIME; //...inicializa contagem do tempo que se mant�m o bot�o carregado
        }break;


      case CLOSE_AUTO: // 5 - Fecha at� a persiana ficar completamente fechada
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB &= ~(1<<DIR); // com dire��o para baixo (fecha)

        if ( (!height) || CloseBtn){ // Se j� fechou completamente ou voltou-se a carregar no bot�o de fechar
          state = IDLE; // para de fechar
        }
        else if (OpenBtn){ // Se carregar no bot�o de abrir posteriormente a persiana volta a abrir
          state = OPEN_CHECK; // Abre persiana e...
          check_delay = CHECK_TIME; // ...inicializa contagem do tempo que se mant�m o bot�o carregado
        }break;


      case CLOSE_MANUAL: // 6 - Fecha persiana at� deixar de premir o bot�o (ou fechar completamente)
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB &= ~(1<<DIR); // com dire��o para baixo (fecha)

        if (!CloseBtn || !height || OpenBtn){ // Se j� fechou ou se deixou de carregar no bot�o ou se carregou no bot�o de abertura
          state = IDLE; // para de fechar
        }break;



      case OPEN_MANUAL: // 7 - Abre persiana at� deixar de premir o bot�o (ou abrir completamente)
        PORTB &= (~(1<<MOTOR)); // Ligar motor
        PORTB |= (1<<DIR); // com dire��o para cima (abre)

        if (!OpenBtn || (MAX_HEIGHT == height) || CloseBtn){ // Se j� abriu ou se deixou de carregar no bot�o ou se carregou no bot�o de fecho
          state = IDLE; // para de abrir
        }break;


      case OPEN_X: // 8 - Abre/fecha at� X% da altura (height_reference)
	    if (height > (height_reference)){ // Se a altura atual est� acima da altura de refer�ncia
          PORTB &= ~(1<<MOTOR); // Liga motor
          PORTB &= ~(1<<DIR); // com dire��o para baixo (fecha)
        }
        else if (height < (height_reference)){ // Se a altura atual est� abaixo da altura de refer�ncia
          PORTB &= (~(1<<MOTOR)); // Liga motor
          PORTB |= (1<<DIR); // com dire��o para cima (abre)
        }
        else if (height == (height_reference)){ // Se a altura de refer�ncia foi atingida
          state = IDLE; // Para de abrir/fechar
        }break;


      case ILLEGAL:// em casos ilegais o motor � desligado e o sistema entra em bloqueio permanentemente
        PORTB |= (1<<MOTOR); // desliga motor
        break;

      default:// para qualquer caso fora do previsto, transita para o estado ilegal e desliga motor
        PORTB |= (1<<MOTOR); // desliga motor
        state = ILLEGAL; // transita para estado ilegal
    }

    #ifdef DEBUG
      if (state != printfstate){
        printf("((STATE:%d; height:%d; Input:%c; check_delay:%u; OPEN:%d  CLOSE:%d  MOTOR:%d  DIR %d))\n",state, height, USB_input, check_delay, OpenBtn ,CloseBtn ,!(PINB & (1<<MOTOR)) ,(PINB & (1<<DIR)));
        printfstate = state;
      }
    #endif

  }
}
