/*
 * main.c
 *  Um programa de controlo de persiana com comunicação por porta
 *  série.
 *
 *  Objetivo:
 *  Controlar uma persiana usando dois botões (cima e baixo), se
 *  se carregar rapidamente num dos botões a persiana abre/fecha
 *  completamente (modo automático). Se se carregar lentamente num
 *  dos botões a persiana abre/fecha até se deixar de carregar no
 *  respetivo botão (modo manual). Alternativamente, por
 *  computador (ou qualquer dispositivo capaz de enviar tramas por
 *  porta série para o Arduino com código ASCII) poder controlar o
 *  nível de abertura da persiana (fechada, aberta, apenas separar
 *  tábuas ou abrir até uma determinada percentagem).
 *
 *  Pormenores contextuais:
 *   -No tempo em que ainda não se decidiu entre um clique
 *  "rápido" e um clique "lento" a persiana já se encontra em
 *  movimento.
 *   -Estando a persiana a abrir num sentido, em modo automático,
 *  se se carregar no botão de sentido oposto, a persiana obedece
 *  mudando de direção.
 *   -Estando a persiana a abrir num sentido, em modo automático,
 *  se se carregar no botão do mesmo sentido, a persiana para.
 *   -Estando a persiana a abrir num sentido, em modo manual, se
 *  se carregar no botão de sentido oposto, a persiana para e
 *  aguarda até que um novo botão seja novamente carregado.
 *   -Os comandos por porta série têm prioridade absoluta em
 *  relação aos botões (comandos nunca serão ignorados nem
 *  adiados). E mesmo estando numa operação causada por comando
 *  de porta série, um novo comando será sempre imediatamente
 *  respondido.
 *   -Considerou-se que o tempo que a persiana demora a abrir
 *  é de 13,2s (cronometrado) e que a sua velocidade de subida
 *  permanece constante, o que não é verdade e gera um erro
 *  cumulativo.
 *   -Considerou-se que a persiana consegue mudar de direção
 *  enquanto o motor está ligado, sem qualquer problema e sem
 *  atrasos. Infelizmente, como a mudança de direção implica uma
 *  mudança brusca da fase a que a persiana está ligada, esta
 *  troca pode, eventualmente, estragar a persiana. Deve-se,
 *  assim, evitar estas transições perigosas.
 *
 *  Estrutura do código:
 *   Este código, tal como a grande maioria de códigos
 *   com o intuido de automatizar, é constituído de uma secção de
 *   inicialização que estabelece uma base para que o programa
 *   possa funcionar (estabelecer entradas/saídas, configurar
 *   timers e comunicação por porta série) seguido de um ciclo que
 *   corre infinitamente que contém a aplicação propriamente dita.
 *   Este ciclo segue uma lógica de máquina de estados, usando uma
 *   variável que contém um número simbólico indicativo do estado
 *   atual, que por sua vez representa em que situação se encontra
 *   a persiana. Qualquer comando por porta série irá forçar o
 *   estado a "obedecer" ao comando, independentemente do estado
 *   atual (com exceção do estado de inicialização), sendo assim
 *   que se estabelece a prioridade a estes comandos. As saídas
 *   (motor e sua direção) são controladas no início de cada estado
 *   com o fim de evitar uma estrutura "switch" separada para o
 *   efeito.
 *
 *  Timer:
 *   Havia alguma liberdade com a escolha da base de tempo para o
 *   timer, no entanto, e prevendo  que controlar a persiana
 *   através do seu tempo de subida iria gerar um erro
 *   significativo, escolheu-se uma base de tempo de 1ms para
 *   atenuar este erro.
 *   Com uma base de 1ms a solução de maior prescaler, TP, e
 *   prescaler de "Clock" a 1 (para evitar afetar outros timers e
 *   processos), é com uma contagem de 125 (obtido por Excel):
 *    CP * TP * CNT = Fcpu * Tint
 *    1  * 128* 125 = 16M  * 1m
 *   Não havendo outras exigências provenientes do timer, será
 *   poupado o timer 1, e implementado o timer 2.
 *   Será implementado o modo normal cujo contador aumenta até
 *   ocorrer overflow, gerando um pedido de interrupção e
 *   reinicializando a 0. Em cada interrupção o contador será
 *   colocado a 125 contagens do seu valor de overflow, de forma
 *   a perservar as 125 contagens necessárias a interrupções
 *   periódicas de 1 ms. Isto é feito atribuindo diretamente à
 *   variàvel do contador o valor referido.
 *   A rotina de interrupção do timer irá decrementar a variável
 *   "check_delay" até esta atingir 0 (age como temporizador) e
 *   irá aumentar ou reduzir a variável "height" conforme o motor
 *   esteja a subir ou a descer (se o motor estiver desligado a
 *   variável permanece inalterada).
 *
 *  Comunicação série:
 *   Utilizando um método de comunicação assíncrona, torna-se
 *   necessário definir uma Baudrate, que foi arbitrada como
 *   sendo de 57600. Vai-se utilizar o modo normal de amostragem,
 *   estabelecendo 16 amostras por cada bit recebido. Sendo a
 *   frequência do cristal associado ao ATMega328p de 16MHz, isto
 *   traduz-se num registo de UBRRO (seguindo fórmula da
 *   datasheet):
 *    UBBRO = 16M/(16*57600)-1 = 16.361 (arredondando) = 16
 *    BAUDRATE = 16M/(16*(16-1)) = 58823,52941 bps
 *   Ou seja, um erro de 2,124% que não será muito significativo
 *   tendo em conta que não serão usados bits de paridade, e
 *   apenas 1 stop bit com 8 bits de dados (9bits no total) que
 *   corresponde a um erro máximo admissível de cerca de 5,56%.
 *   São ativos os registos de leitura e escrita bem como de
 *   interrupção por leitura. Sempre que é lido um valor é gerado
 *   o respetivo pedido de interrupção que simplesmente guarda o
 *   valor recebido numa variável e devolve-o (a devolução do valor
 *   apenas serve para questões informativas ao utilizador). Esta
 *   variável será avaliada no main() de forma a averiguar se é um
 *   input válido e qual o comando a que lhe corresponde, de entre
 *   os seguintes:
 *    'u'-abrir completamente
 *    'g'-colocar as tábuas separadas sem abrir a persiana
 *    '0'~'9'- abrir até x% (0% corresponde a fechar a persiana)
 *
 *  Debug:
 *   O debug não é da nossa autoria tendo sido utilizado para o
 *   efeito a biblioteca "serial.c" da autoria do docente João
 *   Paulo Sousa. Esta biblioteca redireciona a stream stdout
 *   colocando a componente ".put" de FDEV_SETUP_STREAM para a nova
 *   função "usart_putchar()" que escreve, sempre que possível, um
 *   caractere da string indicada por "printf()". A flag de
 *   FDEV_SETUP_STREAM é colocada em modo de escrita
 *   ("_FDEV_SETUP_WRITE"). (".get" não terá nenhum valor pois
 *   apenas se pretende escrever para o PC)
 *   A função "printf_init()" limita-se a atualizar o valor da
 *   variável stdout para a nova configuração descrita.
 *   Para ativar o modo Debug basta definir a constante DEBUG na
 *   linha de código 143.
 *   O código debug leva muito tempo a correr em relação ao resto
 *   do programa, e é apenas usado aquando do teste da persiana
 *   na fase de desenvolvimento.
 *
 *  Cabeçalho creado em: 26/11/2018 (pós avaliação presencial)
 *  Código creado em: 15/11/2018
 *      Autores: Carlos Manuel Santos Pinto
 *               Maria Sara Delgadinho Noronha
 */

#include <avr/interrupt.h>
#include "serial.h"

// DEBUG mode
//#define DEBUG

// Nomes simbolicos para os estados
#define INIT 0 // Inicialização
#define IDLE 1 // Aguarda Comandos
#define CLOSE_CHECK 2 // Fecha e decide entre manual e automático
#define OPEN_CHECK 3 // Abre e decide entre manual e automático
#define OPEN_AUTO 4 // Abre automaticamente
#define CLOSE_AUTO 5 // Fecha automaticamente
#define CLOSE_MANUAL 6 // Fecha manualmente
#define OPEN_MANUAL 7 // Abre manualmente
#define OPEN_X 8 // Abre/fecha até X% da altura máxima (altura de referência)
#define ILLEGAL 255 // Para estados imprevistos

#define T2BOTTOM 255-125 // Valor inicial de contagem de timer 2 (ver função "config_timer2()")

#define MOTOR PB0 // Posição respetiva ao pino do motor (ativo a 0)
#define DIR PB1 // Posição respetiva ao pino da direção do motor (0 vai para cima, 1 vai para baixo)
#define CLOSE PD6 // Posição respetiva ao pino do botão de fecho (ativo a 0)
#define OPEN PD7 // Posição respetiva ao pino do botão de abertura (ativo a 0)

#define CHECK_TIME 500 // 0.5s para distinguir entre clique rápido e lento
#define INIT_TIME 14000 // 14s para fechar totalmente (garantidamente)

#define MAX_HEIGHT 13200 //13.2s para chegar à máxima altura (cronometrado - sujeito a erro)
#define OPEN_TIME 2500 // Corresponde ao tempo que a persiana demora a começar a abrir (tábuas deixam de tocar na base, também foi cronometrado)
#define OPEN_10 (MAX_HEIGHT-OPEN_TIME)/10 // Valor relativo (10%) de abertura descontando o tempo de abertura definido na linha anterior


#ifndef F_CPU
#define F_CPU 16000000ul // 16MHz de frequência de relógio do processador (para estabelecer a Baudrate)
#endif

#define BAUD 57600ul // Baudrate de 57600 simbolos/s
#define UBBR_VAL ((F_CPU/(BAUD*16))-1) // 16 amostras por símbolo (modo normal)

uint8_t OpenBtn = 0; // Variável auxiliar de verificação (Botão de abertura pressionado -> 1)
uint8_t CloseBtn = 0; // Variável auxiliar de verificação (Botão de fecho pressionado -> 1)
uint8_t RE_OpenBtn = 0; // Rising Edge de OpenBtn
uint8_t RE_CloseBtn = 0; // Rising Edge de CloseBtn
volatile uint8_t USB_input = 0; // Variável auxiliar que vai guardar o caracter recebido por porta série
uint8_t state = INIT; // Estado atual
unsigned int height_reference = 0; // Altura de referência (apenas pertinente no estado OPEN_X)
volatile unsigned int height = MAX_HEIGHT; // Altura atual, inicia no topo
volatile unsigned int check_delay = INIT_TIME; // Tempo de espera na decisão entre clique rápido e lento. Também é
                                               // usado como timer na inicialização para garantir que a persiana fecha
#ifdef DEBUG
uint8_t printfstate = 254; // Último estado impresso por printf (apenas pertinente no caso de debug)
#endif

/* Configura pinos de entrada/saída */
void config_io (void){
  DDRB |= ((1<<MOTOR) | (1<<DIR)); // configura os pinos respetivos ao motor e sua direção como saídas
  DDRD &= (~(1<<CLOSE) & ~(1<<OPEN)); // configura os pinos respetivos aos botões de abertura/fecho como entradas

  PORTB |= (1<<MOTOR); // Garante que o motor está, inicialmente, desligado
}

/* Configura timer 2 para contar em ciclos de 1ms:
 * Frequência do CPU = 16MHz;
 * TP = 128; CP = 1; CNT = 125 impulsos (255-130);
 * onde 130 é o valor inicial de contagem para que
 * sejam contados 125 impulsos até overflow.
 * Tempo entre interrupções é dado por:
 *   128*125/16M = 1ms */
void config_timer2 (void){
  TCCR2B = 0; // Para o timer
  TIFR2 |= (7<<TOV2); // Desliga quaisquer flags que estejam ativas
  TCCR2A = 0; // Modo de contagem normal
  TCNT2 = T2BOTTOM; // Posiciona o inicio da contagem no valor estabelecido em T2BOTTOM
  TIMSK2 = (1<<TOIE1); // Permite interrupção por overflow
  TCCR2B = 5; // Inicia o timer com um prescaler : TP=128
}

void init_usart(){
	// Configuração do conjunto de bits para determinar frequência de transição entre bits
	UBRR0 = UBBR_VAL;
	/* Significa que cada bit será amostrado 16 vezes
	 * Que por sua vez significa que teremos uma
	 * Baudrate efetiva de 58823.52941 Bps */

	UCSR0C = (3<<UCSZ00) // Cada segmento terá 8 bits de informação útil (data bits),
		   | (0<<UPM00)  // sem paridade,
		   | (0<<USBS0); // e 1 bit de paragem

	UCSR0B = (1<<TXEN0) | (1<<RXEN0) | (1<<RXCIE0); // Permite leitura, escrita e interrupção após leitura
}

ISR (USART_RX_vect) { // Sempre que recebe dados por porta série
 USB_input = UDR0; // ATMega recebe os dados do PC e guarda-os
 UDR0 = USB_input; // Envia os dados que recebeu, de volta para o PC
}

ISR (TIMER2_OVF_vect){ // Interrupção gerada a cada 1ms
  TCNT2 = T2BOTTOM; // atualiza valor inicial de contagem do timer 2

  if ( !(PINB & (1<<MOTOR)) && !(PINB & (1<<DIR)) && (height)){ // se o motor estiver a fechar
    height--; // decrementa altura
  }
  else if ( !(PINB & (1<<MOTOR)) && (PINB & (1<<DIR)) && (height < MAX_HEIGHT)){ // se o motor estiver a abrir
    height++; // incrementa altura
  }

  if (check_delay){ // se check_delay ainda não atingiu 0
    check_delay--; // decrementa
  }
}



int main(){

  init_usart(); // Configura a comunicação por porta série
  config_io(); // Configura pinos de entrada e saída
  config_timer2(); // Configura timer 2
  sei(); // Ativar bit geral de interrupções, permitindo interrupções em geral

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

    /* A porta série tem prioridade sobre os butões, portanto assim que algo é lido, é
     * processado o que foi recebido e a máquina de estados é forçada ao estado adequado */
    if(USB_input!=0){ // Se algo foi lido por porta série força a máquina de estados a responder de acordo
      if (INIT!=state){
        if ('u' == USB_input){ // Se se premiu "u", abre completamente
          state = OPEN_AUTO; // Abre (completamente) em modo automático
        }
        else if ('0' == USB_input) { // Se se premiu "0", fecha completamente
          state = CLOSE_AUTO; // Fecha (completamente) em modo automático
        }
        else if (USB_input>'0' && USB_input<='9'){ // Foi premido um número que não zero
          height_reference = OPEN_10*(USB_input-48)+OPEN_TIME; // Toma valores desde 10% a 90% de abertura, dependendo da tecla premida
          state = OPEN_X; // Abre/fecha até height_reference
        }
        else if (USB_input == 'g'){ // Se se premiu "g", separa as tábuas sem abrir a persiana
          height_reference = OPEN_TIME; // Altura de abertura efetiva da persiana
          state = OPEN_X; // Abre/fecha até ficar com as tábuas separadas
        }
      }

      USB_input = 0; // Reset da variável de leitura
    }

    switch (state){
      case INIT: // 0 - Inicialização (fecha totalmente a persiana e ignora comandos do utilizador)
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB |= (1<<DIR); // com direção para cima (abre)

        if (!check_delay){ // Se já passou tempo de inicialização (check_delay é usado como timer de inicialização apenas nesta linha)
          state = IDLE; // para de fechar e aguarda comandos do utilizador
        }break;

      case IDLE: // 1 - Espera por qualquer ação (Inativo)
        PORTB |= (1<<MOTOR); // desliga motor

        if (RE_CloseBtn && height ){ // Quer-se fechar e ainda não está fechado nem se está a carregar no botão de abrir
          state = CLOSE_CHECK; // Fecha persiana e...
          check_delay = CHECK_TIME; //...inicializa contagem do tempo que se mantém o botão carregado
        }
        else if (RE_OpenBtn && (MAX_HEIGHT != height) ){ // Quer-se abrir e ainda não está aberto
          state = OPEN_CHECK; // Abre persiana e...
          check_delay = CHECK_TIME; // ...inicializa contagem do tempo que se mantém o botão carregado
        }break;


      case CLOSE_CHECK: // 2 - Fecha e verifica quanto tempo se prime o botão
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB &= ~(1<<DIR); // com direção para baixo (fecha)

    	if (!height){ // Se já fechou completamente
    	  state = IDLE; // para de fechar
    	}
        else if (!CloseBtn){ // Se já não se está a carrergar no botão (implica que foi um clique rápido)
          state = CLOSE_AUTO; // fecha em modo automático
        }
        else if (!check_delay){ // Se já passou o tempo (implica que foi um clique lento)
          state = CLOSE_MANUAL; // fecha em modo manual
        }
        else if (OpenBtn) { // Se se carregar no botão de abertura
          state = IDLE; // Para de fechar
        }break;


      case OPEN_CHECK: // 3 - Abre e verifica quanto tempo se prime o botão
        PORTB &= (~(1<<MOTOR)); // Liga motor
        PORTB |= (1<<DIR); // com direção para cima (abre)

        if (MAX_HEIGHT == height){ // Se já abriu completamente
          state = IDLE; // para de abrir
        }
        else if (!OpenBtn){ // Se já não se está a carrergar no botão (implica que foi um clique rápido)
          state = OPEN_AUTO; // abre em modo automático
        }
        else if (!check_delay){ // Se já passou o tempo (implica que foi um clique lento)
          state = OPEN_MANUAL; // abre em modo manual
        }
        else if (CloseBtn){ // Se se carregar no botão de fecho
          state = IDLE; // Para de abrir
        }break;


      case OPEN_AUTO: // 4 - Abre até a persiana ficar completamente aberta
        PORTB &= (~(1<<MOTOR)); // Liga motor
        PORTB |= (1<<DIR); // com direção para cima (abre)

        if (MAX_HEIGHT == height || OpenBtn){ // Se já abriu completamente ou voltou-se a carregar no botão de abrir
          state = IDLE; // para de abrir
        }
        else if(CloseBtn){ // Se se carregou no botão de fecho
          state = CLOSE_CHECK; // Fecha persiana e...
          check_delay = CHECK_TIME; //...inicializa contagem do tempo que se mantém o botão carregado
        }break;


      case CLOSE_AUTO: // 5 - Fecha até a persiana ficar completamente fechada
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB &= ~(1<<DIR); // com direção para baixo (fecha)

        if ( (!height) || CloseBtn){ // Se já fechou completamente ou voltou-se a carregar no botão de fechar
          state = IDLE; // para de fechar
        }
        else if (OpenBtn){ // Se carregar no botão de abrir posteriormente a persiana volta a abrir
          state = OPEN_CHECK; // Abre persiana e...
          check_delay = CHECK_TIME; // ...inicializa contagem do tempo que se mantém o botão carregado
        }break;


      case CLOSE_MANUAL: // 6 - Fecha persiana até deixar de premir o botão (ou fechar completamente)
        PORTB &= ~(1<<MOTOR); // Liga motor
        PORTB &= ~(1<<DIR); // com direção para baixo (fecha)

        if (!CloseBtn || !height || OpenBtn){ // Se já fechou ou se deixou de carregar no botão ou se carregou no botão de abertura
          state = IDLE; // para de fechar
        }break;



      case OPEN_MANUAL: // 7 - Abre persiana até deixar de premir o botão (ou abrir completamente)
        PORTB &= (~(1<<MOTOR)); // Ligar motor
        PORTB |= (1<<DIR); // com direção para cima (abre)

        if (!OpenBtn || (MAX_HEIGHT == height) || CloseBtn){ // Se já abriu ou se deixou de carregar no botão ou se carregou no botão de fecho
          state = IDLE; // para de abrir
        }break;


      case OPEN_X: // 8 - Abre/fecha até X% da altura (height_reference)
	    if (height > (height_reference)){ // Se a altura atual está acima da altura de referência
          PORTB &= ~(1<<MOTOR); // Liga motor
          PORTB &= ~(1<<DIR); // com direção para baixo (fecha)
        }
        else if (height < (height_reference)){ // Se a altura atual está abaixo da altura de referência
          PORTB &= (~(1<<MOTOR)); // Liga motor
          PORTB |= (1<<DIR); // com direção para cima (abre)
        }
        else if (height == (height_reference)){ // Se a altura de referência foi atingida
          state = IDLE; // Para de abrir/fechar
        }break;


      case ILLEGAL:// em casos ilegais o motor é desligado e o sistema entra em bloqueio permanentemente
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
