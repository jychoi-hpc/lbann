import pytest
import common_code

def error_if(f, f_symbol, data_field, actual_values, expected_values, model_name, errors, all_values, frequency_str):
  d = actual_values[data_field]
  for model_id in sorted(d.keys()):
    for epoch_id in sorted(d[model_id].keys()):
      actual_value = d[model_id][epoch_id]
      expected_value = expected_values[epoch_id][data_field + frequency_str]

      if actual_value == None:
        errors.append('d[%s][%s] == None' % (model_id, epoch_id))
      if expected_value == None:
        errors.append('d[%s]([%s] == None' % (model_id, epoch_id))

      if f(actual_value, expected_value):
        errors.append('%f %s %f %s Model %s Epoch %s %s' % (actual_value, f_symbol, expected_value, model_name, model_id, epoch_id, data_field))
      all_values.append('%f %s Model %s Epoch %s %s' % (actual_value, model_name, model_id, epoch_id, data_field))

def run_tests(actual_objective_functions, model_name, dir_name, cluster, should_log, compiler_name, frequency_str=''):
    expected_objective_functions = common_code.csv_to_dict('%s/bamboo/integration_tests/expected_values/%s/%s/expected_%s_objective_functions.csv' % (dir_name, cluster, compiler_name, model_name))
    errors = []
    all_values = []
    tolerance = 0.05
    # Are we within tolerance * expected_value?
    outside_tolerance = lambda x,y: abs(x - y) > abs(tolerance * y)
    error_if(outside_tolerance, '!=', 'training_objective_function', actual_objective_functions, expected_objective_functions, model_name, errors, all_values, frequency_str)

    print('Errors for: %s %s (%d)' % (model_name, compiler_name, len(errors)))
    for error in errors:
        print(error)
    if should_log:
        print('All values for: %s %s (%d)' % (model_name, compiler_name, len(all_values)))
        for value in all_values:
            print(value)
    assert errors == []

DATA_FIELDS = [
  'training_objective_function'
]

def skeleton_autoencoder_mnist(cluster, dir_name, executables, compiler_name):
  if compiler_name not in executables:
    pytest.skip('default_exes[%s] does not exist' % compiler_name)    
  model_folder = 'models/autoencoder_mnist'
  model_name = 'conv_autoencoder_mnist'
  should_log=False
  actual_objective_functions = common_code.skeleton(cluster, dir_name, executables[compiler_name], model_folder, model_name, DATA_FIELDS, should_log, compiler_name=compiler_name)
  run_tests(actual_objective_functions, model_name, dir_name, cluster, should_log, compiler_name)

def skeleton_autoencoder_imagenet(cluster, dir_name, executables, compiler_name, weekly):
  if cluster == 'surface':
    pytest.skip('skeleton_autoencoder_imagenet does not run on surface')
  if compiler_name not in executables:
    pytest.skip('default_exes[%s] does not exist' % compiler_name)
  model_folder = 'models/autoencoder_imagenet'
  model_name = 'conv_autoencoder_imagenet'
  should_log = False
  actual_objective_functions = common_code.skeleton(cluster, dir_name, executables[compiler_name], model_folder, model_name, DATA_FIELDS, should_log, compiler_name=compiler_name, weekly=weekly)
  frequency_str = '_nightly'
  if weekly:
    frequency_str = '_weekly'
  run_tests(actual_objective_functions, model_name, dir_name, cluster, should_log, compiler_name, frequency_str)

def test_integration_autoencoder_mnist_clang4(cluster, dirname, exes):
  if cluster in ['catalyst', 'quartz']:
    pytest.skip('FIXME')
    # Catalyst Errors:
    # 0.219298 != 0.207480 conv_autoencoder_mnist Model 0 Epoch 0 training_objective_function
  skeleton_autoencoder_mnist(cluster, dirname, exes, 'clang4')

def test_integration_autoencoder_imagenet_clang4(cluster, dirname, exes, weekly):
  skeleton_autoencoder_imagenet(cluster, dirname, exes, 'clang4', weekly)
  
def test_integration_autoencoder_mnist_gcc4(cluster, dirname, exes):
  if cluster in ['catalyst', 'quartz', 'surface']:
    pytest.skip('FIXME')
    # Catalyst Errors:
    # 0.219298 != 0.207480 conv_autoencoder_mnist Model 0 Epoch 0 training_objective_function
    # Surface Errors:
    # 0.053411 != 0.207587 conv_autoencoder_mnist Model 0 Epoch 0 training_objective_function
    # 0.026719 != 0.194595 conv_autoencoder_mnist Model 0 Epoch 1 training_objective_function
    # 0.024882 != 0.193141 conv_autoencoder_mnist Model 0 Epoch 2 training_objective_function
    # 0.023039 != 0.192808 conv_autoencoder_mnist Model 0 Epoch 3 training_objective_function
    # 0.023243 != 0.192716 conv_autoencoder_mnist Model 0 Epoch 4 training_objective_function
  skeleton_autoencoder_mnist(cluster, dirname, exes, 'gcc4')

def test_integration_autoencoder_imagenet_gcc4(cluster, dirname, exes, weekly):
  skeleton_autoencoder_imagenet(cluster, dirname, exes, 'gcc4', weekly)
  
def test_integration_autoencoder_mnist_gcc7(cluster, dirname, exes):
  if cluster in ['catalyst', 'quartz']:
    pytest.skip('FIXME')
    # Catalyst Errors:
    # 0.219383 != 0.207514 conv_autoencoder_mnist Model 0 Epoch 0 training_objective_function
  skeleton_autoencoder_mnist(cluster, dirname, exes, 'gcc7')

def test_integration_autoencoder_imagenet_gcc7(cluster, dirname, exes, weekly):
  skeleton_autoencoder_imagenet(cluster, dirname, exes, 'gcc7', weekly)
  
def test_integration_autoencoder_mnist_intel18(cluster, dirname, exes):
  skeleton_autoencoder_mnist(cluster, dirname, exes, 'intel18')

def test_integration_autoencoder_imagenet_intel18(cluster, dirname, exes, weekly):
  skeleton_autoencoder_imagenet(cluster, dirname, exes, 'intel18', weekly)

# Run with python -m pytest -s test_integration_autoencoder.py -k 'test_integration_autoencoder_mnist_exe' --exe=<executable>
def test_integration_autoencoder_mnist_exe(cluster, dirname, exe):
    if exe == None:
        pytest.skip('Non-local testing')
    exes = {'exe' : exe}
    skeleton_autoencoder_mnist(cluster, dirname, exes, 'exe', True)

# Run with python -m pytest -s test_integration_autoencoder.py -k 'test_integration_autoencoder_imagenet_exe' --exe=<executable>
def test_integration_autoencoder_imagenet_exe(cluster, dirname, exe):
    if exe == None:
        pytest.skip('Non-local testing')
    exes = {'exe' : exe}
    skeleton_autoencoder_imagenet(cluster, dirname, exes, 'exe', True)