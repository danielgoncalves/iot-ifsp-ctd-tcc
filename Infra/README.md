# Infraestrutura de Serviços

Este documento descreve o passo-a-passo para criação da infraestrutura de serviços de software necessária para operação e monitoramento das unidades via navegador web. Essa infraestrutura requer:

* Um servidor MQTT para distribuição e enfileiramento de mensagens, através do qual as unidades enviam e recebem notificações, em um modelo em que os equipamentos se inscrevem em determinados tópicos para receber notificações ou publicam dados para enviar mensagens de notificação de eventos.
* Um servidor PostgreSQL para registro permanente (log) dos eventos de leitura e dos eventos de escoamento.
* Uma instância de aplicação do Node-RED que disponibiliza a criação e manutenção do fluxo de mensagens de maneira visual, em que "nós" especializados recebem, opcionalmente modificam, e repassam ou representam diversos dados. Nesta mesma instância,  podemos ter acesso à um painel que permite visualizar os dados além de interagir com as unidades.

A implementação da infraestrutura será baseada na tecnologia de contêineres [Docker](https://www.docker.com/) para virtualizar as instâncias dos serviços em um *host* Linux.

---
## Rede

Primeiro, crie uma rede Docker em modo *bridge*, chamada `iot-network`:

```shell
$ docker network create --driver bridge iot-network
```

---
## MQTT Server

Este servidor permite que determinadas aplicações cliente publiquem mensagens em tópicos específicos e que outras aplicações inscrevam-se nestes tópicos para serem notificadas das mensagens enviadas. Dessa forma, as aplicações podem comunicar-se, recebendo e enviando notificações entre si.

* Implementação: [Eclipse Mosquitto](https://mosquitto.org/)
* Imagem: https://hub.docker.com/_/eclipse-mosquitto
* Preparação dos diretórios para mapeamento do volume no servidor MQTT:

```shell
$ sudo mkdir -p /mosquitto/config
$ sudo mkdir /mosquitto/data
$ sudo mkdir /mosquitto/log
$ sudo vim /mosquitto/config/mosquitto.conf
```

>[!warning] 
>Note que `/mosquitto` é criado na raiz do sistema de arquivos, de propriedade do `root`

* Conteúdo do arquivo `/mosquitto/config/mosquitto.conf`:

```plaintext
persistence true
persistence_location /mosquitto/data/
log_dest file /mosquitto/log/mosquitto.log

allow_anonymous true
listener 1883 0.0.0.0
```

* Baixe a imagem do `eclipse-mosquitto:2.0.20` e inicie um container, mapeando as portas 1883 e 9001 do host para o container:

```shell
$ docker pull eclipse-mosquitto:2.0.20
$ docker run \
    --restart=unless-stopped \
    --network=iot-network \
    --name=iot-mqtt-server \
    -p 1883:1883 \
    -p 9001:9001 \
    -v /mosquitto:/mosquitto \
    -d eclipse-mosquitto
```

---
## PostgreSQL

Este servidor de banco de dados será utilizado para registrar de forma persistente os eventos de leitura dos níveis de fluidos e dos eventos de acionamento das bombas para escoamento.

* Implementação: [PostgreSQL](https://www.postgresql.org/)
* Imagem: https://hub.docker.com/_/postgres
* Usaremos um [volume Docker](https://docs.docker.com/engine/storage/volumes/), ao invés de mapear um diretório do host:

```shell
$ docker volume create iot-postgres-data
```

* Baixe a imagem do `postgres:12-alpine` e inicie um container, mapeando a porta padrão do postgres, 5432 do host para o container:

```shell
$ docker pull postgres:12-alpine
$ docker run \
    --restart=unless-stopped \
    --network=iot-network \
    --name=iot-postgres-server \
    -p 5432:5432 \
    -v iot-postgres-data:/var/lib/postgresql/data \
    -e "POSTGRES_PASSWORD=SUPERSECRETA" \
    -d postgres:latest
```

>[!info] Senha
>Defina uma senha que conforme seus requisitos de segurança.

### Configuração e criação do banco de dados

* Acesse o container: `$ docker exec -it iot-postgres-server /bin/bash`
* Edite: `# vi /var/log/postgresql/data/pg_hba.conf`
* Inclua as linhas:

```plaintext
# IPv4 local connections
host    all    all    127.0.0.1/32    md5
host    all    all    10.0.0.0/24     md5  # permite acesso via rede local
host    all    all    172.17.0.0/24   md5  # rede Docker
host    all    all    172.18.0.0/24   md5  # rede Docker bridge (verifique via ifconfig)
```

* Crie o banco de dados `containers`:
 
```
# su - postgres
# createdb containers
```

* Use `psql` para ter acesso ao banco de dados por dentro do container:

```
# psql -d containers
containers=# \?             <- help
containers=# \conninfo      <- mostra dados da conexão
containers=# \dt            <- mostra lista de relações do tipo `table`
containers=# \ds            <- mostra lista de relações do tipo `sequence`
containers=# \d+ table_name <- mostra colunas de `table_name`
containers=# \q             <- quit
```

---
## Node-RED

Trata-se de uma aplicação que permite programar diversos fluxos de dados, de origens diversas, de forma visual. Essa aplicação será a base para receber, transformar e agrupar os dados emitidos pelas unidades, possibilitando monitorar e interagir através de um painel construído especificamente para este projeto.

* Implementação: [Node-RED](https://nodered.org/)
* Imagem: https://hub.docker.com/r/nodered/node-red
* Preparação do diretório para mapeamento do volume na aplicação:

```shell
$ sudo mkdir -p /nodered/data
$ sudo chown -R 1000:1000 /nodered
```

>[!warning] 
>Note que `/nodered` é criado na raiz do sistema de arquivos, de propriedade do `root`

* Baixe a imagem do Node-RED e inicie um container, mapeando a porta 1880 no host:

```shell
$ docker pull nodered/node-red:4.0.5
$ docker run \
    --restart=unless-stopped \
    --network=iot-network \
    --name=iot-nodered-webapp \
    -p 1880:1880 \
    -v /nodered/data:/data \
    -d nodered/node-red
```

* Configure autenticação por senha:

```shell
$ vim /nodered/data/settings.js
```

* Descomente toda a parte:

```json
{
    adminAuth: {
        type: "credentials",
        users: [{
            username: "admin",
            password: "$2b$08$74SpEgfO1f4QEJhs8Wv...nPl5F6f6EJK",
            permissions: "*"
        }]
    }
}
```

* Para criar um hash de uma senha:

```shell
$ docker exec -it nodered-webapp /bin/bash

# no container
$ node-red admin hash-pw
Password: 
HASHED-PASSWORD
```

Copie `HASHED-PASSWORD` e cole em `/nodered/data/settings.js` em `adminAuth/type/users/0/password`.

---
## Reiniciação necessária

Após as mudanças de configurações feitas no PostgreSQL e de acesso na aplicação Node-RED, é necessário reiniciar estes serviços para que as mudanças tenham efeito:

```shell
$ docker restart iot-postgres-server
$ docker restart iot-nodered-webapp
```

Verifique que os containers estão em execução com o comando `docker ps`, observando a coluna `STATUS`.

---
## Aplicação Node-RED

Acesse `127.0.0.1:1880` no navegador e continue a preparar a aplicação para que seja possível utilizar o fluxo já existente e deixar a infraestrutura completamente operacional.

### Dependências

O fluxo já existente precisa que as seguintes dependências, chamadas **nodes**, sejam instaladas:

* [node-red-contrib-postgresql](https://flows.nodered.org/node/node-red-contrib-postgresql) `0.14.2`
* [node-red-dashboard](https://flows.nodered.org/node/node-red-dashboard) `3.6.5`
* [node-red-node-random](https://flows.nodered.org/node/node-red-node-random) `0.4.1`
* [node-red-node-ui-table](https://flows.nodered.org/node/node-red-node-ui-table) `0.4.4`

Clique no menu (𝍢) e escolha a opção "Gerenciar Paleta". Clique na guia "Instalar" e faça a instalação dos **nodes** relacionados acima.

### Importação do fluxo

O arquivo com o fluxo Node-RED para este projeto está em  `iot-ifsp-ctd-tcc/Infra/Node-RED/flow.json`. Clique no menu (𝍢) e escolha a opção "Importar" para importar esse fluxo.

![Interface do Node-RED](/Imagens/Infra/node-red-ui.png)

### Configuração node: MQTT

Dê um clique duplo em qualquer um dos **nodes** MQTT in ou MQTT out e revise as configurações de conexão ao servidor MQTT. Normalmente, o endereço do host será o endereço IP da máquina local (máquina host). Verifique com `ifconfig`. A configuração é propagada automaticamente para os outros **nodes** do mesmo tipo.

### Configuração node: PostgreSQL

Dê um clique duplo em qualquer um dos **nodes** PostgreSQL e revise as configurações de conexão ao servidor MQTT. Normalmente, o endereço do host será o endereço IP da máquina local (máquina host). Verifique com `ifconfig`. A configuração é propagada automaticamente para os outros **nodes** do mesmo tipo.

### Deployment

Clique no botão "Implementação" (ou "Deploy") no canto superior direito para compilar e deixar o fluxo ativo e disponível.

### Dashboard

A interface do painel de monitoramento, o *dashboard*, está acessível em `127.0.0.1:1880/ui`.

![Interface do Dashboard](/Imagens/Infra/node-red-ui-dashboard.png)

