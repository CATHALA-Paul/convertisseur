import dash
from dash import dcc
from dash import html
from callbacks import register_callbacks
import dash_bootstrap_components as dbc  # Import Dash Bootstrap components

app = dash.Dash()

# Create the layout using Dash Bootstrap components
app.layout = dbc.Container(
    fluid=True,
    children=[
        dbc.Col(
                (dcc.Graph(id='subplot-graph', style={"height": "90vh"}),
                dbc.Button("Reload", id="button", name = "Reload", className="mt-3"),  # Add margin top for spacing
                ), style={"height": "100vh"},  # Set the height of the container to full viewport height
        ),
    ],
)

register_callbacks(app)

if __name__ == '__main__':
    app.run()
