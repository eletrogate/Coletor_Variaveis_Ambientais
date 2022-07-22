/**********************************************************************
*           COLETOR DE DADOS DE VARIÁVEIS ATMOSFÉRICAS                *
*                  J. D. S. BERNARDO JULHO 2021                       *      
*                     josedaniel10@gmail.com                          *
***********************************************************************/

#include <SPI.h> //BIBLIOTECA PARA INTERFACE SPI
#include <Wire.h> //BIBLIOTECA PARA INTERFACE I2C

#include <LiquidCrystal_I2C.h> //BIBLIOTECA DO DISPLAY LCD VIA MÓDULO I2C
LiquidCrystal_I2C lcd(0x27,16,2); //(I2C END, COL, LIN)

#include <Adafruit_BME280.h> //BIBLIOTECA PARA O MÓDULO I2C BME280,
Adafruit_BME280 bme;         //SENSOR DE TEMP., UMID. RELAT. E PRESS. ATM. 

#include <RTClib.h> //BIBLIOTECA PARA O MÓDULO I2C RTC (RELÓGIO DE TEMPO REAL)
RTC_DS3231 rtc;

#include "SdFat.h" //BIBLIOTECA PARA O MÓDULO SPI DE CARTÃO SD 
// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 0
SdFat SD;
File datalog;
#define SPI_SPEED SD_SCK_MHZ(50) //PODE SER NECESSÁRIO DIMINUIR A VELOCIDADE (<50) 
                                 //PARA ALGUNS CARTÕES SD

char temp_s[5], rh_s[4], p_atm_s[5]; //VARIÁVEIS AMBIENTAIS: TEMP., UMID. RELAT., PRESS. ATM.
char dia[3], mes[3], ano[3], hora[3], minuto[3], segundo[3]; //TEMPO FORMATADO
bool pisca_pontos; //PISCAR PONTOS DA HORA
char data_arq_ini[7]; //DATA INICIAL
char data_arq_nova[7]; //DATA ATUAL 
bool st_sd = 0; //FLAG DE STATUS DO CARTÃO SD:  0 ->NOK e 1 ->OK
bool sp_sd = 1; //FLAG DE ESPAÇO NO SD: 0 -> SEM ESPAÇO e 1 -> COM ESPAÇO
bool st_bme; //FLAG DE STATUS (OK=1 OU NOK=0) DO SENSOR BME280
bool msg_err_bme = 1; //IMPEDE REPETIÇÃO DE MSG ERRO BME280 A CADA SEGUNDO (MOSTRA SÓ UMA VEZ)
                  //1-> BME NOK  0-> BME OK
bool st_rtc = 0; //FLAG DE STATUS DO RTC:  0 ->NOK e 1 ->OK
bool msg_err_rtc = 1; //IMPEDE REPETIÇÃO DE MSG ERRO RTC A CADA SEGUNDO (MOSTRA SÓ UMA VEZ)
                  //1-> RTC NOK  0-> RTC OK
                  
byte min_ini;  //MOMENTO DE GRAVAÇÃO
byte min_atual;  //MOMENTO DE GRAVAÇÃO
byte cont_msg_err_rtc; //CONTAGEM PARA REPETIÇÃO DA MSG ERRO RTC A CADA MINUTO


//CRIAÇÃO DOS CARACTERES ESPECIAIS PARA O LCD
//BITS EM 1 PONTOS ACESOS; EM ZERO, APAGADOS

//CARACTERE ºC
byte grau[8] = {
  B11100,
  B10100,
  B11100,
  B00011,
  B00100,
  B00100,
  B00100,
  B00011,
};

//CARACTERE UR
byte ur[8] = {
  B10100,
  B10100,
  B11100,
  B00111,
  B00101,
  B00111,
  B00110,
  B00101,
};

//CARACTERE mb
byte mb[8] = {
  B01010,
  B10101,
  B10101,
  B00000,
  B00100,
  B00110,
  B00101,
  B00110,
};

void setup() 
{
  pisca_pontos = 1; //INVERSÃO, A CADA SEGUNDO, DOS PONTOS SEPARADORES DE HORA E MINUTO

  pinMode(2, OUTPUT); //CONFIGURA O PINO 2 COMO SAÍDA DIGITAL (LED VERDE HEART BIT)
  pinMode(3, OUTPUT); //CONFIGURA O PINO 3 COMO SAÍDA DIGITAL (LED VERMELHO FALHA)

  //INICIALIZAÇÃO DA PORTA SERIAL PARA MOSTRAR MENSAGENS NA TELA DO MICRO
  Serial.begin(9600);
  Serial.println("COLETOR DE DADOS AMBIENTAIS V1");

  //INICIALIZAÇÃO DO SENSOR BME280
  st_bme = bme.begin(0x77); //ESTE SENSOR ADMITE DOIS ENDEREÇOS NO BARRAMENTO I2C: 76h OU 77h
  //st_bme = bme.begin(0x76); 
  if (!st_bme) 
     Serial.println("ERRO: sensor BME280 não localizado");

  //INICIALIZAÇÃO DO LCD 
  lcd.init();   
  lcd.backlight(); //BACKLIGHT DO LCD ACIONADA

  //CRIAÇÃO DOS CARACTERES ESPECIAIS PARA O LCD
  lcd.createChar(0, grau); //0 = CARACTERE grau (ºC)
  lcd.createChar(1, ur); //1 = CARACTERE UR (UMIDADE RELATIVA) 
  lcd.createChar(2, mb); //2 = CARACTERE mb (mili bar)

  //MENSAGEM INICIAL
  lcd.setCursor(0,0);// (COLUNA, LINHA)
  lcd.print("VARIAVEIS");
  lcd.setCursor(3,1);// (COLUNA, LINHA)
  lcd.print("AMBIENTAIS V1");
  delay(3000);
  
  //INICIALIZAÇÃO DO RELÓGIO DE TEMPO REAL (RTC)
  
   data_hora();
   min_ini = min_atual;   
   
  //DEFINE A CONFIGURAÇÃO DE TRANSFERÊNCIA DE DADOS DO BARRAMENTO SPI
  SPI.setDataMode(SPI_MODE3);  

  pinMode(10, OUTPUT);    // CS (CHIP SELECT) SD CARD
  digitalWrite(10,HIGH);  // CS SD CARD
  
  init_sd_card();  //INICIALIZAÇÃO DO CARTÃO SD
 

/*
//***********************************************************************************
  //ACERTA O RELÓGIO DO RTC, COM O HORÁRIO DO COMPUTADOR NO MOMENTO 
  //DA COMPILAÇÃO E GRAVAÇÃO DO PROGRAMA NO MÓDULO 
  //PARA SER EXECUTADO APENAS: 
  // a) NA PRIMEIRA VEZ EM QUE SE UTILIZAR O RTC, 
  // b) QUANDO FOR NECESSÁRIO TROCAR A BATERIA OU 
  // c) QUANDO O RTC ESTIVER COM A HORA/DATA ERRADAS
  // APÓS O ACERTO DO RTC, COMENTAR NOVAMENTE ESTE TRECHO E RECOMPILAR/GRAVAR NO MÓDULO 
        
        Serial.print("Acerto de hora:    ");
        Serial.print( F(__DATE__) ); 
        Serial.print("   ");
        Serial.println( F(__TIME__) );
        rtc.adjust(DateTime( F(__DATE__), F(__TIME__) ) ); 
//***********************************************************************************  
*/

}

void dateTime(uint16_t* date, uint16_t* time) //PARA GRAVAR O ARQUIVO NO SD 
                                              //COM A DATA/HORA DO RTC
{
  
  if(rtc.begin())
    { 
      DateTime now = rtc.now();

     // RETORNA A DATA UTILIZANDO A MACRO FAT_DATE PARA FORMATAR OS CAMPOS
     *date = FAT_DATE(now.year(), now.month(), now.day());
  
     // RETORNA A HORA UTILIZANDO A MACRO FAT_TIME PARA FORMATAR OS CAMPOS
     *time = FAT_TIME(now.hour(), now.minute(), now.second());
    }
}


void init_sd_card() //FUNÇÃO PARA INICIALIZAR E VERIFICAR O ESTADO DO CARTÃO SD
{
  if (!SD.begin(10)) //INICIALIZA CARTÃO SD
    {
     Serial.println("ERRO: falha no cartão SD");
     st_sd = 0; //SD NOK
    }
  else
     st_sd = 1; //SD OK
}


void var_atm() //FUNÇÃO QUE LÊ AS VARIÁVEIS ATMOSFÉRICAS DO SENSOR BME280
{
   float temp; //TEMPERATURA (ºC)
   byte rh; //UMIDADE RELATIVA DO AR (%)
   int p_atm; //PRESSÃO ATMOSFÉRICA (mb)
     
   st_bme = bme.begin(0x77); //ESTE SENSOR ADMITE DOIS ENDEREÇOS NO BARRAMENTO I2C: 76h OU 77h
       
   if (st_bme) //SENSOR BME280 OK
   {
     msg_err_bme = 1;
     temp = bme.readTemperature(); //LEITURA DA TEMPERATURA
     rh = bme.readHumidity(); //LEITURA DA UMIDADE RELATIVA
     p_atm = bme.readPressure()/100; //LEITURA E CONVERSÃO DA PRESSÃO ATMOSFÉRICA 
    
     if(temp > -9.9 && temp < 85) //INTERVALO DE INDICAÇÃO DE TEMPERATURA: DE -9,9ºC A 85ºC
       {
        dtostrf(temp, 4, 1, temp_s);  //CONVERTE FLOAT PARA STRING
        temp_s[2] = ','; //SUBSTITUI O PONTO DECIMAL DA STRING POR VÍRGULA
       }
     else 
       sprintf(temp_s,"%c%c%c%c",'-','-','-','-'); //SE A MEDIÇÃO ESTIVER FORA DA FAIXA 
                                                   //REGISTRA "----"
       
     sprintf(rh_s,"%2d",rh); //CONVERTE E FORMATA DE BYTE PARA STRING
     sprintf(p_atm_s,"%4d",p_atm); //CONVERTE E FORMATA DE INT PARA STRING
   }
   else if(!st_bme && msg_err_bme) //SENSOR BME280 EM FALHA E MSG DE ERRO HABILITADA
   {
     msg_err_bme = 0;
     Serial.println("ERRO: leitura do sensor BME280");
     sprintf(temp_s,"%c%c%c%c",'-','-','-','-'); //SE OCORRER FALHA NA LEITURA DO SENSOR BME280
     sprintf(rh_s,"%c%c",'-','-');               //REGISTRA "-" NAS VARIÁVEIS
     sprintf(p_atm_s,"%c%c%c%c",'-','-','-','-');
   }
          
}

void data_hora() //FUNÇÃO QUE EFETUA A LEITURA DE DATA E HORA DO RTC 
                 //E CONVERSÃO/FORMATAÇÃO PARA STRINGS
{
   if(rtc.begin())
   {
     st_rtc = 1;
     msg_err_rtc = 1;
     
     DateTime now = rtc.now(); //EFETUA A LEITURA DE DADOS DO RTC

     //SE DIA E MÊS TIVEREM APENAS UM DÍGITO, UM ZERO É ACRESCENTADO NA FRENTE DELE    
     if(now.day() < 10) 
       sprintf(dia,"0%1d",now.day());
     else 
       sprintf(dia,"%2d",now.day());
    
     if(now.month() < 10) 
       sprintf(mes,"0%1d",now.month());
     else 
       sprintf(mes,"%2d",now.month());
  
     sprintf(ano,"%2d",now.year()-2000);
  
     if(now.hour() < 10)   
       sprintf(hora,"0%1d",now.hour());
     else 
       sprintf(hora,"%2d",now.hour());
     
     if(now.minute() < 10) 
       sprintf(minuto,"0%1d",now.minute());
     else 
       sprintf(minuto,"%2d",now.minute());
     
     min_atual = now.minute();
   }
  else if(msg_err_rtc) //RTC EM FALHA E MSG DE ERRO HABILITADA
   {
     st_rtc = 0;
     msg_err_rtc = 0;
     sprintf(dia,"%c%c",'-','-'); 
     sprintf(mes,"%c%c",'-','-'); 
     sprintf(ano,"%c%c",'-','-'); 
     sprintf(hora,"%c%c",'-','-'); 
     sprintf(minuto,"%c%c",'-','-'); 
     Serial.println("ERRO: RTC Relógio de Tempo Real");      
   }

  if(!st_rtc) //SE ERRO RTC INCREMENTA VARIÁVEL PARA REPETIÇÃO
     cont_msg_err_rtc++;
     
  if(cont_msg_err_rtc > 50) //SE VARIÁVEL PARA REPETIÇÃO > 50 (~1 MINUTO)
   {
     cont_msg_err_rtc = 0;
     msg_err_rtc = 1;  //HABILITA MOSTRAR MSG ERRO RTC
   }
}


void sd_card() //FUNÇÃO QUE EFETUA A GRAVAÇÃO DE DADOS NO SD
{
 init_sd_card(); //REATIVA ACESSO SD (A BIBLIOTECA SdFat NÃO MOSTRA ERRO 
                 //QUANDO O SD ERA RETIRADO); TEM DE SER TESTADO SEMPRE
 
 if(st_sd && st_bme) //SE SD OK E BME OK       
   {
    SdFile::dateTimeCallback(dateTime); //PARA SALVAR O ARQUIVO NO SD COM A DATA DO RTC
    
    char nome_arq[13];  //NOME DO ARQUIVO A SER SALVO NO SD:
                        //MÁXIMO 8 CARACTERES + '.' + EXTENSÃO "csv"
    
    sprintf(data_arq_nova, "%s%s%s", ano, mes, dia); //FORMATA DATA CORRENTE COMO AAMMDD 
                                                     //PARA FACILITAR A INDEXAÇÃO DOS ARQUIVOS

   //FORAM USADAS CONVERSÕES DE STRING PARA LONG (ATOL)
   //PORQUE AS STRINGS AAMMDD TEM 6 DÍGITOS E SÃO MAIORES QUE UM INTEIRO (INT)
   if(atol(data_arq_nova) > atol(data_arq_ini))  //se a data no momento da gravação for superior à lida na 
      {                                          //inicialização, atualiza data de inicialização 
        sprintf(nome_arq, "%s%s%s", "VA", data_arq_nova, ".csv"); //NOME ARQUIVO = DATA ATUALIZADA
                                                                  //VAAAMMDD.csv
        sprintf(data_arq_ini, "%s", data_arq_nova);               //(PASSOU DE 1/2 NOITE)  
      }
   else
        sprintf(nome_arq, "%s%s%s", "VA", data_arq_ini, ".csv"); //NOME ARQUIVO = DATA DA INICIALIZAÇÃO
                                                                //(NÃO PASSOU DE 1/2 NOITE) 
 
   datalog = SD.open(nome_arq, FILE_WRITE); //ABRE ARQUIVO PARA GRAVAÇÃO
              
   if (datalog) //SE ABERTURA DE ARQUIVO OK
     {
         //GRAVA DADOS NO SD
         datalog.print(dia);
         datalog.print("/");
         datalog.print(mes);
         datalog.print("/");
         datalog.print(ano);
         datalog.print(" ");
         datalog.print(hora);
         datalog.print(":");
         datalog.print(minuto);
         datalog.print(" ");
         datalog.print(temp_s);
         datalog.print(" ");
         datalog.print(rh_s);
         datalog.print(" ");
         datalog.println(p_atm_s);
         
         datalog.close(); //FECHA ARQUIVO 

         Serial.println(nome_arq);
         Serial.print(dia);
         Serial.print("/");
         Serial.print(mes);
         Serial.print("/");
         Serial.print(ano);
         Serial.print(" ");
         Serial.print(hora);
         Serial.print(":");
         Serial.print(minuto);
         Serial.print(" ");
         Serial.print(temp_s);
         Serial.print(" ");
         Serial.print(rh_s);
         Serial.print(" ");
         Serial.println(p_atm_s);
      } 
  else //SE HOUVE FALHA NA ABERTURA/ESCRITA NO SD 
      {
        Serial.print("ERRO: abertura de arquivo");
        Serial.println(nome_arq);
      
        st_sd = 0; //SD NOK
      }
   }
}

void escreve_lcd()
{
    lcd.setCursor(0,0);// (COLUNA, LINHA) //LINHA 0: DATA E HORA
    lcd.print(" ");
    lcd.print(dia);
    lcd.print("/");
    lcd.print(mes);
    lcd.print("/");
    lcd.print(ano);
    lcd.print(" ");
    lcd.print(hora);
       
    if(pisca_pontos)
       lcd.print(":");
    else
       lcd.print(" ");
    pisca_pontos = !pisca_pontos;
    lcd.print(minuto);
    lcd.print(" ");
        
    lcd.setCursor(0,1); //(COLUNA, LINHA)
    lcd.print(temp_s);
    lcd.write(byte(0)); //SÍMBOLO ºC
    lcd.print(" ");
    lcd.print(rh_s);
    lcd.print("%");
    lcd.write(byte(1)); //SÍMBOLO UR
    lcd.print(" ");
    lcd.print(p_atm_s);
    lcd.write(byte(2));  //SÍMBOLO mb
}

void loop() 
{
    if(st_sd && sp_sd && st_bme && st_rtc) //SE NÃO EXISTEM FALHAS
       {
        digitalWrite(2,HIGH); //ACENDE LED VERDE (HEART BIT)
        digitalWrite(3,LOW); //APAGA LED VERMELHO 
       }
    else if(!st_sd || !sp_sd || !st_bme || !st_rtc) //SE EXISTEM FALHAS
        digitalWrite(3,HIGH); //ACENDE LED VERMELHO 
        
    data_hora(); //LEITURA DO RTC
    
    var_atm(); //LEITURA DO BME280
    
    escreve_lcd(); //ESCRITA NO LCD

   //SALVA NO SD AS VARIÁVEIS QUANDO INCREMENTA O MINUTO
   if((min_atual > min_ini) || (min_atual == 0 && min_ini == 59))  
    {
         msg_err_bme = 1; //HABILITA MSG ERRO BME280 UMA VEZ POR MINUTO
         DateTime now = rtc.now(); //LEITURA DE DATA E HORA ATUAIS DO RTC
         byte seg = now.second(); //TEMPO (SEGUNDOS) ANTES DE TENTAR GRAVAR NO SD
         
         digitalWrite(3,HIGH); //ACENDE LED VERMELHO FALHA GRAVAÇÃO SD
         digitalWrite(2,LOW); //APAGA LED VERDE (HEART BIT)
         min_ini = min_atual;
         sd_card(); //SALVA OS RESULTADOS NO SD 
  
         now = rtc.now(); //LEITURA DE DATA E HORA ATUAIS DO RTC
         //SE TEMPO DE GRAVAÇÃO NO SD FOR SUPERIOR A 5 SEGUNDOS SINALIZA ERRO (SD SEM ESPAÇO)
         if(now.second() > seg + 5)
          {
            Serial.println("ERRO: cartão SD sem espaço");
            sp_sd = 0; //SEM ESPAÇO PARA GRAVAÇÃO NO SD
          }
         else
            sp_sd = 1;
    }
      
    digitalWrite(2,LOW); //APAGA LED VERDE (HEART BIT) INDICANDO QUE ROTINA ESTÁ EM EXECUÇÃO
    
    if(!st_sd || !sp_sd || !st_bme || !st_rtc) //SE ESTADO DO SD ESTÁ NOK OU NÃO HÁ ESPAÇO PARA GRAVAR
      digitalWrite(3,LOW); //APAGA LED VERMELHO FALHA GRAVAÇÃO SD
       
    delay(1000); //INTERVALO DE 1 SEGUNDO ATÉ A PRÓXIMA LEITURA DO SENSOR BME280
}
