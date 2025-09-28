# Divisão de Tarefas - Trabalho Prático INF01151

Este documento detalha a separação das implementações necessárias para o trabalho prático, com o objetivo de facilitar a divisão de tarefas em um grupo e minimizar a sobreposição de trabalho.

### 1. Módulos Essenciais e Estruturas Compartilhadas (Base do Projeto)

Esta seção cobre as definições e funcionalidades que tanto o cliente quanto o servidor utilizarão. É uma boa tarefa para um membro do grupo que pode fornecer a base para os outros construírem em cima.

* **1.1. Definição das Estruturas de Pacotes:**
    * Criar os `structs` em C/C++ para todas as mensagens trocadas, conforme a sugestão do documento.
    * Definir os tipos de pacotes, como `DESCOBERTA`, `REQUISIÇÃO`, e seus respectivos `ACKs`.
    * Implementar a estrutura de requisição (`struct requisicao`) contendo o endereço de destino e o valor.
    * Implementar a estrutura de `ack` da requisição (`struct requisicao_ack`), contendo o número de sequência e o novo saldo do cliente.

* **1.2. Módulo de Comunicação (Sockets UDP):**
    * Criar funções encapsuladas para o uso de sockets UDP, que será a API exclusiva de comunicação inter-processos.
    * Implementar uma função para enviar pacotes em modo unicast (ponto-a-ponto).
    * Implementar uma função para enviar pacotes em modo broadcast (difusão), necessária para a descoberta.
    * Implementar funções para receber pacotes de forma genérica.

* **1.3. Compilação e Build (Makefile):**
    * Criar um `Makefile` ou script de compilação automatizado que compile separadamente o cliente e o servidor, conforme exigido para a entrega.

---

### 2. Implementação do Servidor

As tarefas do servidor podem ser divididas entre dois membros: um focado na lógica de rede e processamento e outro focado no gerenciamento de dados e sincronização.

* **2.1. Gerenciamento de Dados e Sincronização (O "Coração" do Servidor):**
    * **Estruturas de Dados:**
        * Implementar a "tabela de clientes participantes", que armazena endereço IP (`address`), último ID de requisição (`last_req`) e saldo (`balance`).
        * Implementar a estrutura para o histórico e saldo total, contendo o número de transações (`num_transactions`), valor total transferido (`total_transferred`) e saldo total do banco (`total_balance`).
    * **Sincronização (Leitor/Escritor):**
        * Implementar o mecanismo de exclusão mútua para acesso concorrente às estruturas de dados, seguindo o modelo leitor/escritor.
        * Garantir que os subserviços de Descoberta e Processamento atuem como "escritores" e o de Interface como "leitor".
        * A escrita não deve ser bloqueante, enquanto a leitura será uma operação bloqueante (o leitor ficará bloqueado até que uma nova atualização esteja disponível).
        * Garantir que nenhum escritor possa modificar a tabela enquanto ela estiver sendo lida.

* **2.2. Subserviço de Descoberta (Escritor):**
    * Operar de forma passiva, aguardando por mensagens do tipo `DESCOBERTA` em broadcast.
    * Ao receber uma descoberta de um novo cliente:
        * Adicionar o cliente à tabela de participantes usando o módulo de sincronização.
        * Inicializar o saldo do novo cliente com um valor N (ex: 100 reais) e o campo `last_req` com zero.
        * Responder ao cliente em unicast para confirmar seu endereço.

* **2.3. Subserviço de Processamento de Requisições (Escritor):**
    * Operar de forma passiva, recebendo mensagens do tipo `REQUISIÇÃO`.
    * Para cada requisição recebida, criar uma nova thread para processá-la de forma concorrente.
    * **Lógica da Thread de Processamento:**
        * Verificar o número de sequência da requisição para garantir que é o próximo esperado.
        * Se o `req_id` for o esperado, processar a transação: verificar saldo, debitar da origem, creditar no destino, e atualizar o histórico e saldo total do banco.
        * Se o `req_id` for duplicado (menor que o esperado), responder com o `ack` da última requisição processada.
        * Se o `req_id` for maior que o esperado, responder com o `ack` do último número recebido para indicar pacotes perdidos.
        * Se o saldo for insuficiente, a transação não ocorre e o servidor deve informar o cliente pagante.
        * Enviar uma resposta (`ack`) ao cliente de origem, confirmando o processamento e informando o novo saldo.

* **2.4. Subserviço de Interface do Servidor (Leitor):**
    * Receber a porta UDP como parâmetro de linha de comando.
    * Ao iniciar, exibir a mensagem de status inicial com data, hora e valores zerados no formato exato especificado.
    * Atuar como "leitor" que aguarda por atualizações na tabela.
    * A cada nova transação processada, exibir o log no formato exato, incluindo data, hora, IPs, `req_id`, valor e os totais atualizados.
    * Para requisições duplicadas, exibir a mensagem de log com a marcação "DUP!!", também no formato exato.

---

### 3. Implementação do Cliente

A implementação do cliente pode ser atribuída a um único membro do grupo.

* **3.1. Subserviço de Descoberta (Ativo):**
    * Ao iniciar, enviar uma mensagem `DESCOBERTA` em modo broadcast para encontrar o servidor.
    * Aguardar a resposta unicast do servidor para obter seu endereço IP.

* **3.2. Lógica de Requisições e Retransmissão:**
    * Manter um contador local para o número de sequência (`req_id`), iniciando em 1 e incrementando a cada *nova* requisição enviada.
    * Enviar uma requisição por vez, aguardando a confirmação (`ack`) antes de enviar a próxima.
    * Implementar um mecanismo de `timeout` (ex: 10 milissegundos).
    * Caso o `timeout` expire sem receber um `ack`, o cliente deve assumir que a mensagem foi perdida e reenviá-la.

* **3.3. Subserviço de Interface do Cliente (Entrada e Saída com Threads):**
    * Receber a porta UDP como parâmetro de linha de comando.
    * Após a descoberta, exibir o endereço do servidor no formato exato especificado.
    * **Implementação com duas threads**:
        * **Thread de Entrada do Usuário:** Ler comandos do `stdin` no formato `IP_DESTINO VALOR`. Esta thread não deve exibir nenhum prompt na tela. Ela é responsável por construir e iniciar o envio da requisição.
        * **Thread de Saída/Exibição:** Responsável por exibir na tela as respostas (`ack`) recebidas do servidor no formato exato especificado.
    * Implementar o encerramento do processo ao receber `CTRL+C` ou `CTRL+D`.

---

### 4. Relatório e Finalização

Esta é uma tarefa de documentação que pode ser realizada em conjunto ou por um membro designado.

* **4.1. Escrita do Relatório Técnico:**
    * Produzir um relatório contendo todos os itens solicitados.
    * Explicar a implementação de cada subserviço, as estruturas de dados, as primitivas de comunicação e as justificativas das decisões.
    * Detalhar em quais áreas do código foi necessário garantir sincronização.
    * Descrever os problemas encontrados durante o desenvolvimento e como foram resolvidos.
    * Especificar claramente a contribuição de cada integrante do grupo no trabalho.
