from flask import Flask, render_template

from bokeh.client import pull_session
from bokeh.embed import server_session
from bokeh.embed import components

app = Flask(__name__)

@app.route('/', methods=['GET'])
def bkapp_page():

    # pull a new session from a running Bokeh server
    with pull_session(url="http://localhost:5006/bokehserver") as session:

        # update or customize that session
        session.document.roots[0].children[1].title.text = "Special Sliders For A Specific User!"

        s, d = components(session.document.roots[0])
        # generate a script to load the customized session

        script = server_session(session_id=session.id, url='http://localhost:5006/bokehserver')
        session.connect()
        # use the script in the rendered page
        return render_template("chart.html", sessionscript=script, template="Flask")

#if __name__ == '__main__':
#    app.run(port=8080)
