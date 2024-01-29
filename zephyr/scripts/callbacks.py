import numpy as np
# import dash
# from dash import dcc
# from dash import html
from dash.dependencies import Input, Output
from plotly.subplots import make_subplots
import plotly.graph_objs as go
from pyocd.core.helpers import ConnectHelper
from pyocd.flash.file_programmer import FileProgrammer
from pyocd.debug.elf.symbols import ELFSymbolProvider

# Replace ST_LINK_ID with the ID of your st-link, on the command promt
# enter "pyocd list" to get the ID.
ST_LINK_ID='0039002E3331510633323639'
cst_time = (1/170)
options = {'connect_mode': 'attach', 'target_override': 'stm32g474retx'}
session = ConnectHelper.session_with_chosen_probe(ST_LINK_ID,   options=options)
session.open()
session.target.elf = '../.pio/build/spin/firmware.elf'
provider = ELFSymbolProvider(session.target.elf)

def compose(rise, fall):
    edges = np.concatenate((rise, fall))
    edges.sort(kind='mergesort') #merge the 2 arrays to get an array of edges

    xaxis = []
    yaxis = []
    prevValue = 0

    for e in edges:
        yaxis.append(prevValue)
        xaxis.append(e)
        if (e in rise):
            yaxis.append(1)
            prevValue = 1
        elif (e in fall):
            yaxis.append(0)
            prevValue = 0
        xaxis.append(e)
    return (xaxis, yaxis)

def register_callbacks(app):

    @app.callback(
        Output('subplot-graph', 'figure'),
        Input('button', 'n_clicks'),
    )
    def update_graphs(n_intervals):
        # Get the address of record_exti8_CHA1
        addr_rise_DD = provider.get_symbol_value("record_rise_data_dispatch") 
        addr_fall_DD = provider.get_symbol_value("record_fall_data_dispatch") 

        # Get the address of 
        addr_rise_CT = provider.get_symbol_value("record_rise_control_task")
        addr_fall_CT = provider.get_symbol_value("record_fall_control_task")
        
        fig = make_subplots(rows=2, cols=2, vertical_spacing=0.1,
                            subplot_titles=['Scatter CT', 'Histogram CT', 'Scatter DD', 'Histogram DD'])
        
        RECORD_SIZE = 2047
        # Read data from memory
        datas_rise_DD = session.target.read_memory_block32(addr_rise_DD, RECORD_SIZE) 
        datas_fall_DD = session.target.read_memory_block32(addr_fall_DD, RECORD_SIZE) 

        datas_rise_CT = session.target.read_memory_block32(addr_rise_CT, RECORD_SIZE) 
        datas_fall_CT = session.target.read_memory_block32(addr_fall_CT, RECORD_SIZE) 
        # session.close()

        # Convert the data to numpy arrays for easier manipulation
        np_rise_DD = np.array(datas_rise_DD, dtype=np.float32)
        np_fall_DD = np.array(datas_fall_DD, dtype=np.float32)
        np_rise_CT = np.array(datas_rise_CT, dtype=np.float32)
        np_fall_CT = np.array(datas_fall_CT, dtype=np.float32)

        # Multiply data_array by cst_time
        np_rise_DD *= cst_time
        np_fall_DD *= cst_time
        np_rise_CT *= cst_time
        np_fall_CT *= cst_time

        offset = np_rise_DD[1]
        np_rise_DD -= offset
        np_fall_DD -= offset
        np_rise_CT -= offset
        np_fall_CT -= offset

        # Compute time differences between consecutive data points
        time_diffs_DD = np_fall_DD - np_rise_DD 
        # Compute time differences between consecutive data points
        time_diffs_CT = np_fall_CT - np_rise_CT

        # Calculate mean and standard deviation 
        mean_time_diffs_DD = np.mean(time_diffs_DD)
        std_time_diffs_DD = np.std(time_diffs_DD)

        mean_time_diffs_CT = np.mean(time_diffs_CT)
        std_time_diffs_CT = np.std(time_diffs_CT)

        xpoints_CT, ypoints_CT = compose(np_rise_CT[1:], np_fall_CT[1:])
        xpoints_DD, ypoints_DD = compose(np_rise_DD[1:], np_fall_DD[1:])

        # Scatter plots
        fig.add_trace(go.Scatter(x=xpoints_CT, y=ypoints_CT, mode='lines', name='CT'), row=1, col=1)
        fig.add_trace(go.Scatter(x=xpoints_DD, y=ypoints_DD, mode='lines', name='DD'), row=2, col=1)

        # Histograms
        fig.add_trace(go.Histogram(x=time_diffs_CT[1:], name='CT Time Diffs', nbinsx=100), row=1, col=2)
        fig.add_trace(go.Histogram(x=time_diffs_DD[1:], name='DD Time Diffs', nbinsx=100), row=2, col=2)

        fig.update_layout(
            xaxis1 = dict(range=[0,450]),
            xaxis2 = dict(range=[min(time_diffs_CT[1:]),max(time_diffs_CT[1:])]),
            xaxis3 = dict(range=[0,450]),
            xaxis4 = dict(range=[min(time_diffs_DD[1:]),max(time_diffs_DD[1:])]),
            hovermode='x'  # Set hovermode to 'x' to show cursor on hover along x-axis
        )

        return fig



