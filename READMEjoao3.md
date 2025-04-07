# Alterações feitas:

## Comparação entre unisgned int e int em socketEngine.h
Solução adotada: Fazer static casting na comparação

## Conditional_Data_Observer apresenta método virtual, porém seu destrutor não é virtual
Solução adotada: Fazer do destrutor um método virtual na classe Conditional_Data_Observer

## Erro capturado por tipo em demo.cpp
Solução adotada: Erros passam a ser capturados por referencia

## g++ aponta erro por mudar o significado de 'Classe' em typedef do tipo typedef Classe<C> Classe
Solução adotada: Mudar a declaração para typedef Classe<C> (prefixo)Classe

## No seu construtor, protocolo inicializa _paddr antes de _port, apesar de _port ser declarado antes
Solução adotada: Mudar a ordem da inicialização

## Segmentation fault localizado em NIC<SocketEngine>::alloc():
O erro parece ocorrer em função de Buffer -> clear() e inicialmente ocorria em buf->data()->src = address(); por uma tentativa de escrever dados em um endereço inválido.
Tentativa de solução:
Como o erro parecia ser não haver uma struct do tipo Ethernet::Frame corretamente inicializado no local de memória já que Buffer<T>::clear() realiza memset(data, 0, size). Inicializa-se uma nova struct antes de fazer a atribuição:
nic.h linha 219:
Ethernet::Frame init_frame;
    init_frame.src = {};
    init_frame.src = {};
    init_frame.prot = 0;
    std::memset(&init_frame, 0, Ethernet::MTU);
    buf -> setData(&init_frame, sizeof(Ethernet::Frame));

Com isso o segmentation fault passa a ocorrer em buf -> setData(&init_frame, sizeof(Ethernet::Frame)); na tentativa de atribuição ao atributo _size da classe Buffer. Com isso entende-se que o ponteiro para buffer pode ser nulo.
Tentativa de solução:
nic.h linha 213
if (buf == nullptr) {
    std::cerr << "Warning/Error: NIC::alloc failed, no free buffers available." << std::endl;
    return nullptr;
}

Com isso quando um ponteiro para buffer retirado da fila é na verdade um ponteiro nulo a alocação falha. Porém a falha continua acontecendo. Ocorrendo então em DataBuffer* buf = _free_buffers.front(); (nic.h 209) por tentativa de leitura em um endereço inválido. A fila só é acessada em 3 situações: 
- Em sua inicialização (no construtor da classe NIC)
- Na alocação de um buffer (onde o segmentation fault acontece)
- Quando um buffer é liberado em NIC<SocketEngine>::free()
Como o segmentation fault só ocorre mais a frente na execução a causa do erro deveria estar em NIC<SocketEngine>::free():
template <typename Engine>
void NIC<Engine>::free(DataBuffer* buf) 
{
db<NIC>(TRC) << "NIC<Engine>::free() called!\n";

if (!buf) return;

buf->clear();
db<NIC>(INF) << "[NIC] buffer released\n";

pthread_mutex_lock(&_buffer_mtx);
_free_buffers.push(buf);
pthread_mutex_unlock(&_buffer_mtx);

sem_post(&_buffer_sem);
}

Como a função apenas chama buf -> clear() e o recoloca na fila de buffers livres, voltamos ao ponto onde a provavel cause é buf -> clear(). Apesar disso garantir que o array Buffer._data é um array de zeros é indicado para a confiabilidade do programa.