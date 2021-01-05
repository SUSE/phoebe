# Phoeβe

## Idea
<p>
Phoeβe wants to add base artificial intelligence capabilities to the Linux OS.
</p>

## Architecture
<p>
Phoeβe is designed with a plugin architecture in mind, providing an interface for new functionalities to be added at ease.<br><br>
Plugins are loaded at runtime and registered with the main body of execution. The only requirement is to implement the interface dictated by the structure <i>plugin_t</i>. The <strong>network_plugin.c</strong> represents a very good example of how to implement a new plugin for Phoeβe.
</p>

## Disclaimer
<p>
The mathematical model implemented is a super basic one, which contemplates a <i>machine-learning 101</i> approach: 
<strong>input * weight + bias</strong>. It does not use any fancy techniques and the complexity is close to zero.<br><br>
The plan is to eventually migrate towards a model created in Tensorflow and exported so to be used by Phoeβe but 
we are not there yet.
</p>

## 10,000 feet view
<p>
The code allows for both <strong>training</strong> and <strong>inference:</strong> all the knobs which can 
modify the run-time behaviour of the implementation are configurable via the <i>settings.json</i> file,
where each parameter is explained in details.
</p>

<p>
For the inference case, when a match is found, then the identified kernel parameters are configured accordingly.<br><br>
The inference loop runs every N seconds and the value is configurable via the <strong>inference_loop_period</strong>. 
Depending on how quick we want the system to react to a situation change, then the value given to the 
<strong>inference_loop_period</strong> will be bigger or smaller.<br><br>
The code has a dedicated stats collection thread which periodically collects system statistics and populates structures 
used by the inference loop. The statistics are collected every N seconds and the value is configurable via the 
<strong>stats_collection_period</strong>. Depending on the overall network demands, the value of 
<strong>stats_collection_period</strong> will be bigger or smaller to react slower or quicker to network events.
</p>

<p>
In case a high traffic rate is seen on the network and a matching entry is found, then the code will not consider
any lower values for a certain period of time: the value is configurable via the <strong>grace_period</strong> in
the <i>settings.json</i> file. <br>
That behaviour has been implemented to avoid causing too much reconfiguration on the system and to prevent
sudden system reconfiguration due to network spikes.
</p>

<p>
The code also supports few approximation functions, also available via the <i>settings.json</i> file.<br>
The approximation functions can tune the tolerance value - runtime calculated - to further allow the user for fine 
tuning of the matching criteria. Depending on the approximation function, obviously, the matching criteria could be
narrower or broader.
</p>

## Settings

Below is a detailed an explanation of what configurations are available in settings.json, the possible values and what effect they have. (Note that this is not really valid JSON, please remove the lines with double forward slashes if you use it)

```
{
    "app_settings": {
	// path where application is expecting to find plugins to load
	"plugins_path": "/home/mvarlese/REPOS/ai-poc/bin",

        // max_learning_values: number of values learnt per iteration
        "max_learning_values": 1000,

        // save trained data to file every saving_loop value
        "saving_loop": 10,

        // accuracy: the level of accuracy to find a potential entry
        // given the transfer rate considered.
        //
        // MaxValue: Undefined, MinValue: 0.00..1
        // Probably not very intuitive: a higher number correspondes to
        // a higher accuracy level.
        "accuracy": 0.5,

        // approx_function: the approximation function applied
        // to the calculated tolerance value used to find a
        // matching entry in values.
        //
        // Possible values:
        // 0 = no approximation function
        // 1 = square-root
        // 2 = power-of-two
        // 3 = log10
        // 4 = log
        "approx_function": 0,

        // grace_period: the time which must be elapsed
        // before applying new settings for a lower
        // transfer rate than the one previously measured.
        "grace_period": 10,

        // stats_loop_period: the cadence of time which
        // has to be elapsed between stats collection.
        // It is expressed in seconds but it accepts non-integer
        // values; ie. 0.5 represents half-second
        "stats_collection_period": 0.5,

        // inferece_loop_period: the time which must be
        // elapsed before running a new inference evaluation
        "inference_loop_period": 1

    },
    "weights":{
        "transfer_rate_weight": 0.8,
        "drop_rate_weight" : 0.1,
        "errors_rate_weight" : 0.05,
        "fifo_errors_rate_weight" : 0.05
    },
    "bias": 10
}
```

## Building
<p>
To build the PoC code, it is sufficient to run the command <pre>make</pre> in the top-level folder of the project. There is also a debug build, which uses GCC's address, leak and undefined behavior sanitizers for additioinal checks; it is available with <pre>make BUILD=debug</pre>.<br><br>

There are few compile time flags which can be passed to make to enable some code behaviour:<br>
* <strong>PRINT_MSGS</strong>, used to print to stdout only the most important messages (this is the only parameter enabled by default)<br>
* <strong>PRINT_ADV_MSGS</strong>, used for very verbose printing to stdout (useful for debugging purposes)<br>
* <strong>PRINT_TABLE</strong>, used to print to stdout all data stored in the different tables maintained by the application<br>
* <strong>APPLY_CHANGES</strong>, it enables the application to actually apply the settings via sysctl/ethtool command<br>
* <strong>CHECK_INITIAL_SETTINGS</strong>, when enabled it will prevent the application to apply lower settings than the ones already applied on the system at bootstrap
</p>

## Running
<p>
The code supports multiple mode of operation:

* Training mode:
<pre>./phoebe -f ./csv_files/rates_trained_data.csv -m std-training -s settings.json</pre>

* Live training mode:
<pre>./phoebe -f ./csv_files/rates_trained_data.csv -m live-training -s settings.json</pre>

* Inference
<pre>./phoebe -f ./csv_files/rates_trained_data.csv -i wlan0 -m inference -s settings.json</pre>
</p>

## Feedback / Input / Collaboration
<p>
If you are curious about the project and want to have more information, please, do reach out to <a href="mailto:marco.varlese@suse.com">marco.varlese@suse.com</a>, I will be more than happy to talk to you more about this project and what others initiatives are in this area.<br>
</p>
