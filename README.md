# IoT IFSP CTD

Projeto do trabalho de conclusão da disciplina de Plataformas de Prototipação
para IoT (E2PPT) IFSP CTD.

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

![Diagrama da Unidade de Monitoramento](/Diagramas/diagrama-unidade-monitoramento.png)

### Unidade de Medição

* 1 x ESP32 WROOM Devkit V1;
* 1 x Sensor HC-SR04 ultrassônico para medir o nível do fluído.

![Diagrama da Unidade de Medição](/Diagramas/diagrama-unidade-medicao.png)

### Unidade de Escoamento

* 1 x ESP32 WROOM Devkit V1;
* 1 x Bomba d'água submersível, modelo JT100 (3V a 6V, vazão 80L/h a 120L/h);
* 1 x TIP41C NPN para acionar a bomba d'água;
* 1 x LED (yellow) indicação de acionamento da bomba de escoamento.

![Diagrama da Unidade de Escoamento](/Diagramas/diagrama-unidade-escoamento.png)

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

