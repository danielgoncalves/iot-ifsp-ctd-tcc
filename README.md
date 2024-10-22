# TCC IoT

Projeto de TCC para o curso de pós-graduação lato sensu de Especialização em
Internet das Coisas do Instituto Federal de São Paulo, Campus Catanduva,
iniciado no segundo semestre de 2022.

## Visão Geral

Medidores de nível de fluído em contêineres usando sensores ultrassônicos.
Os dados originados nos sensores são publicados em um tópico via MQTT.
Um segundo conjunto, inscrito no tópico, analisa os níveis e comanda uma
bomba d'água garantindo o escoamento do contêiner correspondente. 
Esse conjunto também pode ser acionado arbitrariamente através de um comando
lido de um segundo tópico.

Um terceiro conjunto contendo um ESP32 e um LCD 15x2 faz a leitura do 
tópico e exibe as leituras no display. Também contém um botão que comanda, 
arbitrariamente, o escoamento do contêiner, enviando um comando em um
segundo tópico.

### Unidade de Monitoramento

* 1 x ESP32 WROOM Devkit V1;
* 1 x Display LCD 15x2 I2C;
* 1 x LED (green) indicação de conectado;
* 1 x LED (blue) indicação de leitura (subscription) ou escrita (published);
* 1 x Push Button (yellow cap) para selecionar o contêiner;
* 1 x Push Button (green cap) para parametrizar o nível do contêiner;
* 1 x Push Button (red cap) para comandar o acionamento arbitrário da bomba de escoamento.

![Diagrama da Unidade de Monitoramento](/Imagens/diagrama-unidade-monitoramento.png)

### Unidade de Medição

* 1 x ESP32 WROOM Devkit V1;
* 1 x Sensor HC-SR04 ultrassônico para medir o nível do fluído.

![Diagrama da Unidade de Medição](/Imagens/diagrama-unidade-medicao.png)

### Unidade de Escoamento

* 1 x ESP32 WROOM Devkit V1;
* 1 x Bomba d'água submersível, modelo JT100 (3V a 6V, vazão 80L/h a 120L/h);
* 1 x TIP41C NPN para acionar a bomba d'água;
* 1 x LED (yellow) indicação de acionamento da bomba de escoamento.

![Diagrama da Unidade de Escoamento](/Imagens/diagrama-unidade-escoamento.png)

Cada contêiner a ser monitorado deve possuir uma unidade de medição e uma
unidade de escoamento.

### Tópicos MQTT

* `p/container/level`
* `p/container/set`
* `p/pump`

#### Tópico `p/container/level`

Onde são publicadas as leituras de nível dos containers.

* Publishers: Unidades de medição;
* Subscribers: Unidades de monitoramento;

```json
{
    "origin": {
        "from": "measureUnit",
        "id": 1
    },
    "level": {
        "ms": 911,
        "cm": 15
    }
}
```

#### Tópico `p/container/set`

Onde são publicados os ajustes de parâmetros de nível dos containers. Por exemplo, uma unidade de monitoramento pode definir que a leitura atual de um determinado container como sendo o limite a partir do qual o escoamento deverá ser comandado.

* Publishers: Unidades de monitoramento;
* Subscribers: Unidades de medição;

```json
{
    "origin": {
        "from": "monitorUnit",
        "id": 1
    },
    "destination": {
        "to": "measureUnit",
        "id": 1
    },
    "level": {
        "ms": 747,
        "cm": 13
    }
}
```

#### Tópico `p/pump`

Onde são publicados os comandos para as unidades de escoamento.

* Publishers: Unidades de monitoramento (para escoamento manual), Unidades de medição (para escoamento a partir do nível determinado);
* Subscribers: Unidades de escoamento;

```json
{
    "origin": {
        "from": "measureUnit",
        "id": 1
    },
    "destination": {
        "to": "pumpUnit",
        "id": 1
    },
    "pump": {
        "seconds": 3
    }
}
```

### Infraestrutura para Monitoramento e Logging

Este projeto conta com uma [infraestrutura em nuvem](https://www.redhat.com/pt-br/topics/cloud-computing/what-is-cloud-infrastructure)
que possibilita o monitoramento e controle remoto dos contêineres mensurados,
via internet, através de um navegador em um PC ou _smartphone_. 
Os serviços disponíveis através de uma instância [EC2](https://aws.amazon.com/pt/ec2/) 
do tipo [t2.micro](https://aws.amazon.com/pt/ec2/instance-types/t2/) onde 
estão virtualizados os serviços através de [containers Docker](https://docs.docker.com/get-started/what-is-a-container/).
Os serviços instalados são:

* Servidor [PostgreSQL](https://hub.docker.com/_/postgres) v15.3
* Servidor [Eclipse Mosquitto](https://hub.docker.com/_/eclipse-mosquitto) v2.0.15
* Servidor [Node-RED](https://hub.docker.com/r/nodered/node-red/) v3.0.2

Através do Node-RED é possível monitorar a comunicação entre os componentes
IoT que compõem o projeto, registrar as leituras em bancos de dados SQL e
fornecer um [painel de monitoramento e comando](http://ec2-18-188-46-54.us-east-2.compute.amazonaws.com:1880/ui/) (_dashboard_).

![Captura das telas do dashboard](/Imagens/dashboard-mobile.png)

O fluxo Node-RED implementado está disponível neste repositório em `Infra/Node-RED/flow.json`. O fluxo originalmente implementado, conforme trabalhos anteriores continua disponível [neste gist](https://gist.github.com/danielgoncalves/37da72b88d048bcf2101a32f4e1452c7), cuja representação na interface da plataforma se parece com a captura de tela:

![Captura de tela do flow do projeto](/Imagens/nodered-main-flow.png)

### Simulações dos Circuitos

Os circuitos estão simulados na plataforma [Wokwi](https://wokwi.com) e 
cada um dos módulos pode ser acessado nos links abaixo:

* [Unidade de Monitoramento](https://wokwi.com/projects/366735052056497153)
* [Unidade de Medição](https://wokwi.com/projects/364468040443813889)
* [Unidade de Escoamento](https://wokwi.com/projects/364470406200202241)

Ao executar a simulação de uma Unidade de Medição não esqueça de modificar a
identificação da unidade em `SONAR_ID` para um número inteiro positivo entre
`1` e `99` (inclusive). É importante que esse número não se repita entre as
Unidades de Medição.

Da mesma forma, ao executar a simulação de uma Unidade de Escoamento não
esqueça de modificar a identificação da unidade em `PUMP_ID` para o mesmo
número da Unidade de Medição a qual está relacionada. 

Diferente das Unidades de Medição, é possível ter mais de uma Unidade de 
Escoamento com o mesmo `PUMP_ID`.

![Simulações dos circuitos em execução](/Imagens/wokwi-exec-simulacoes.png)

Acesse este link para mais detalhes sobre a 
[Rede WiFi ESP32](https://docs.wokwi.com/pt-BR/guides/esp32-wifi) na 
plataforma de simulação Wokwi.

