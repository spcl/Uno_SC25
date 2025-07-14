def filter_true_false(m_name, sub_s):
    if sub_s + '=False' in m_name:
        m_name_splitted = m_name.split(sub_s + '=False')
        return m_name_splitted[0]+m_name_splitted[1]
    if sub_s + '=True' in m_name:
         m_name_splitted = m_name.split(sub_s + '=True')
         return m_name_splitted[0]+sub_s+m_name_splitted[1]
    return m_name
    

def filter_output_name(m_name):
    print(f'Before filtering output name: {m_name}')
    m_name = filter_true_false(m_name, "-noFi")
    m_name = filter_true_false(m_name, "-noQaInter")
    m_name = filter_true_false(m_name, "-noQaIntra")
    m_name = filter_true_false(m_name, "-noRto")
    m_name = filter_true_false(m_name, "-separate")
    m_name = filter_true_false(m_name, "-erasure")
    m_name = filter_true_false(m_name, "-interEcn")
    m_name = filter_true_false(m_name, "-perAckMD")
    m_name = filter_true_false(m_name, "-perAckEwma")
    m_name = filter_true_false(m_name, "-mimd")
    
    print(f'After filtering output name: {m_name}\n\n')
    return m_name